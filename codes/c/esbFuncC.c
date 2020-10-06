// Code that simulates the plant. Time variables are settable.

#include <stdio.h>
#include <math.h>
#include <time.h>

int hraux, minaux, secaux, hr, min, sec, day, mon, year, Sunset, Sunrise, diff, Nsteps = 136*4, DOY, calc, stepsLate, i, newAlarmSec;
float CorrSunset, CorrSunrise, CorrDiff, minPassed, newAlarmMin;

// sunrise_max_error = 1.9399212031896127
int sunrise(int x) {
  return 352.7057070699647 -
    37.874423615159515 * sin(0.016993372856748738 * x + 1.9702732100174332) -
    9.635480273542653 * sin(0.033986745713497475 * x + 3.5210526762558465);
}
// sunset_max_error = 1.6480860263854993
int sunset(int x) {
  return 1077.7684206276278 +
    34.412579844382826 * sin(0.017048572799752065 * x + 1.5651489841528505) -
    9.938477290115724 * sin(0.03409714559950413 * x + 3.6094381149503607);
}

int dayOfTheYear(int day, int mon, int year){	

	int days_in_feb = 28;
	int doy;    // day of year
 
		    
	doy = day;

	// check for leap year
	if( (year % 4 == 0 && year % 100 != 0 ) || (year % 400 == 0) ){
		days_in_feb = 29;
	}

	switch(mon){
		case 2:
		    doy += 31;
		    break;
		case 3:
		    doy += 31+days_in_feb;
		    break;
		case 4:
		    doy += 31+days_in_feb+31;
		    break;
		case 5:
		    doy += 31+days_in_feb+31+30;
		    break;
		case 6:
		    doy += 31+days_in_feb+31+30+31;
		    break;
		case 7:
		    doy += 31+days_in_feb+31+30+31+30;
		    break;            
		case 8:
		    doy += 31+days_in_feb+31+30+31+30+31;
		    break;
		case 9:
		    doy += 31+days_in_feb+31+30+31+30+31+31;
		    break;
		case 10:
		    doy += 31+days_in_feb+31+30+31+30+31+31+30;            
		    break;            
		case 11:
		    doy += 31+days_in_feb+31+30+31+30+31+31+30+31;            
		    break;                        
		case 12:
		    doy += 31+days_in_feb+31+30+31+30+31+31+30+31+30;            
		    break;                                    
	}
	return doy;
}

int calcPerStep(float x){
	float stepPerSeg = x*60/Nsteps;
	return round(stepPerSeg);
}

int HMSToSeg(int Hour, int Minute, int Seconds){
	return Hour*3600 + Minute*60 + Seconds;
}

void secondsToHMS(int seconds, int *h, int *m, int *s)
{
    int t = seconds;

    *s = t % 60;

    t = (t - *s)/60;
    *m = t % 60;

    t = (t - *m)/60;
    *h = t;
}

void delay(int milliseconds)
{
    long pause;
    clock_t now,then;

    pause = milliseconds*(CLOCKS_PER_SEC/1000);
    now = then = clock();
    while( (now-then) < pause )
        now = clock();
}

int main(void){
	while(1){
		printf("Enter date (DD/MM/YYYY): ");
		scanf("%d/%d/%d", &day, &mon, &year);
		printf("Enter time (HH:MM:SS): ");
		scanf("%d:%d:%d", &hr, &min, &sec);
		minPassed = HMSToSeg(hr, min, sec)*1.0/60;
		DOY = dayOfTheYear(day, mon, year);
		printf("DOY: %d\n", DOY);
		Sunrise = sunrise(DOY);
		Sunset = sunset(DOY);
		secondsToHMS(round(Sunrise*60), &hraux, &minaux, &secaux);
		printf("SunriseHMS: %d:%d:%d\n", hraux, minaux, secaux);
		secondsToHMS(round(Sunset*60), &hraux, &minaux, &secaux);
		printf("SunsetHMS: %d:%d:%d\n", hraux, minaux, secaux);
		diff = Sunset-Sunrise;
		CorrSunrise = Sunrise + (17.5/180)*diff;
		secondsToHMS(round(CorrSunrise*60), &hraux, &minaux, &secaux);
		printf("CorrSunriseHMS: %d:%d:%d\n", hraux, minaux, secaux);
		CorrSunset = Sunset - (17.5/180)*diff;
		secondsToHMS(round(CorrSunset*60), &hraux, &minaux, &secaux);
		printf("CorrSunsetHMS: %d:%d:%d\n", hraux, minaux, secaux);
		CorrDiff = CorrSunset - CorrSunrise;
		printf("CorrDiff: %f\n", CorrDiff);
		calc = calcPerStep(CorrDiff);
		printf("Steps Period: %d\n", calc);
		if(minPassed < Sunrise){
			printf("The sun hasnt risen yet.\n");
			secondsToHMS((Sunrise*60), &hraux, &minaux, &secaux);
			printf("Setting next Alarm2 for: %d:%d:%d\n", hraux, minaux, secaux);
		}
		else if(minPassed > Sunset){
			DOY = dayOfTheYear(day+1, mon, year);
			printf("The sun already has set.\n");
			Sunrise = sunrise(DOY);
			Sunset = sunset(DOY);
			diff = Sunset-Sunrise;
			secondsToHMS((Sunrise*60), &hraux, &minaux, &secaux);
			printf("Setting Alarm2 for next DOY, %d: %d:%d:%d\n", DOY, hraux, minaux, secaux);
		}
		else{
			minaux = min+1;
			if(minaux>60){minaux = minaux - 60;}
			printf("Setting next Alarm2 for: %d:%d:%d\n", hr, min+1, 0);	
		}

		if(minPassed < CorrSunrise){
			printf("Its not time to move Nema 17 yet.\n");
			secondsToHMS(round(CorrSunrise*60), &hraux, &minaux, &secaux);
			printf("Setting next Alarm1 for: %d:%d:%d\n", hraux, minaux, secaux);
		}
		else if(minPassed > CorrSunset){
			DOY = dayOfTheYear(day+1, mon, year);
			printf("Its not time to move Nema 17 anymore.\n");
			Sunrise = sunrise(DOY);
			Sunset = sunset(DOY);
			diff = Sunset-Sunrise;
			CorrSunrise = Sunrise + (17.5/180)*diff;
			secondsToHMS(round(CorrSunrise*60), &hraux, &minaux, &secaux);
			printf("Setting Alarm1 for next DOY, %d: %d:%d:%d\n", DOY, hraux, minaux, secaux);
		}
		else{
			stepsLate = floor((minPassed*1.0-CorrSunrise)*60/calc);
			newAlarmSec = round(CorrSunrise*60 + (stepsLate+1)*calc + 0.5*calc);
			secondsToHMS(newAlarmSec, &hraux, &minaux, &secaux);
			printf("The solar panel is late.\n");
			printf("Recovering right position.\n");
			for(i=0; i<stepsLate; i++){
				printf("Moving (%d/%d).\n", (i+1), stepsLate);
				delay(100);
			}
			printf("Setting next Alarm1 for: %d:%d:%d\n", hraux, minaux, secaux);
			
		}
	}
}
