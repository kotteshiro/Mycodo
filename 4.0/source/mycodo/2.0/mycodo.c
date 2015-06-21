/*
*  Mycodo - Regulate temperature and humidity with raspberryPi and
*           DHT22 sensor
*
*  Version: 2.0
*
*  Copyright (C) 2015  Kyle T. Gabriel
*
*  This file is part of Mycodo
*
*  Mycodo is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  Mycodo is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with Mycodo. If not, see <http://www.gnu.org/licenses/>.
*
*  Contact at kylegabriel.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <time.h>
#include <limits.h>
#include <libconfig.h>
#include <unistd.h>

// Main configuration file
char *cwd;
char config_cond_path[1024];
char config_state_path[1024];
char relay_exec_path[1024];
char *config_condition = "/config/mycodo-cond.conf";
char *config_state = "/config/mycodo-state.conf";
char *relay_exec = "/cgi-bin/relay.sh";

// Relay1 HEPA Pin - wiringPi pin 4 is BCM_GPIO 23, GPIO4
// Relay2 HUMI Pin - wiringPi pin 3 is BCM_GPIO 22, GPIO3
// Relay3 CFAN Pin - wiringPi pin 1 is BCM_GPIO 18, GPIO1
// Relay4 HEAT Pin - wiringPi pin 0 is BCM_GPIO 17, GPIO0

#define RHepa 1
#define RHum  2
#define RFan  3
#define RHeat 4
#define SIZE 256

// Constraints of relay control
// If difference between actual temp/hum and set temp/hum is equal or below Xs
// turn on heater/humidifier for Xt seconds, then leave off for 

// Relay 1 Hepa
int RHepat[] = {0, 360, 360, 360, 360, 360};
int RHepao[] = {0, 180, 150, 110, 80, 30};
int RHepaTS = 0;

// Relay 2 Humidifier
int RHums[] = {2, 4, 8, 10};
int RHumt[] = {0, 50, 50, 50, 50, 50};
int RHumo[] = {0, 120, 100, 80, 60, 40};
int RHumTS = 0;

// Relay 3 Circulatory Fan
int RFans[] = {-2, -4, -6, -8, -10};
int RFant[] = {0, 20, 20, 20, 20, 20};
int RFano[] = {0, 100, 100, 100, 100, 100};
int RFanTS = 0;

//Relay 4 Heater
float RHeats[] = {0.3, 0.8, 1.5, 2};
int RHeatt[] = {0, 30, 30, 30, 30, 30};
int RHeato[] = {0, 300, 275, 250, 225, 200};
int RHeatTS = 0;

/* Heater inside chamber
   float RHeats[] = {0.3, 0.8, 1.5, 2};
   int RHeatt[] = {0, 10, 27, 35, 43, 55};
   int RHeato[] = {60, 60, 60, 60, 60};
   int RHums[] = {5, 10, 15, 20};
   int RHumt[] = {0, 5, 10, 15, 20, 30};
   int RHumo[] = {60, 60, 60, 60, 60};
   int RFans[] = {-2, -4, -6, -8, -10};
   int RFant[] = {0, 10, 20, 30, 40, 50};
   int RFano[] = {60, 60, 60, 60, 60};
   int RHepat[] = {0, 10, 20, 30, 40, 50};
 */

double wfactor = 1.0;
int sleept = 60;

int year;
int month;
int day;
int hour;
int min;
int sec;
int timestampL;
double hum;
double temp;

int relay1o = 0;
int relay2o = 0;
int relay3o = 0;
int relay4o = 0;
int relay5o = 0;
int relay6o = 0;
int relay7o = 0;
int relay8o = 0;

int maxTemp = 80;
int minTemp = 70;
int maxHum = 70;
int minHum = 30;
int webOR = 0;
int tempState = 2;
int humState = 2;

int main(int argc, char *argv[]) {
	if ((cwd = getcwd(NULL, 64)) == NULL) {
		perror("pwd");
		exit(2);
	}
	strcat(config_cond_path, cwd);
	strcat(config_cond_path, config_condition);
	
	strcat(config_state_path, cwd);
	strcat(config_state_path, config_state);
	
	strcat(relay_exec_path, cwd);
	strcat(relay_exec_path, relay_exec);
	
	if (argc < 2) {
		printf("Missing input argument!\n");
		printf("Usage: r to read and w to write\n");
		printf("mycodo r\n");
		printf("mycodo w cond minTemp maxTemp minHum maxHum webOR\n");
        printf("mycodo w state tempState humState\n");
		return 1;
	}

	// write input to config file - for web php control interface
	if (strcmp(argv[1], "w") == 0 && strcmp(argv[2], "cond") == 0) {
		readCfgCond();
		minTemp = atoi(argv[3]);
		maxTemp = atoi(argv[4]);
		minHum = atoi(argv[5]);
		maxHum = atoi(argv[6]);
		webOR = atoi(argv[7]);
		writeCfgCond();
	}
	
	// write input to config file - for web php control interface
	else if (strcmp(argv[1], "w") == 0 && strcmp(argv[2], "state") == 0) {
		readCfgState();
		tempState = atoi(argv[3]);
		humState = atoi(argv[4]);
		writeCfgState();
	}
	
	// read config file and print only variable values to screen
	else if (strcmp(argv[1], "r") == 0) {
		readCfgCond();
		readCfgState();
		printf("%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i\n", minTemp, maxTemp, minHum, maxHum, webOR, tempState, humState, relay1o, relay2o, relay3o, relay4o, relay5o, relay6o, relay7o, relay8o);
	}

	else {
		// convert string input to int
		year = atoi(argv[1]);
		month  = atoi(argv[2]);
		day  = atoi(argv[3]);
		hour  = atoi(argv[4]);
		min  = atoi(argv[5]);
		sec  = atoi(argv[6]);
		timestampL = atoi(argv[7]);
		hum = atof(argv[8]);
		temp = atof(argv[9]);

		// read config file and set variables
		printf("\n%s:%s:%s %i Read config file %s",  argv[4], argv[5], argv[6], timestampL, config_cond_path);
		readCfgCond();
		printf("\n%s:%s:%s %i Read config file %s",  argv[4], argv[5], argv[6], timestampL, config_state_path);
		readCfgState();
		printf("\nminTemp: %i, maxTemp: %i, minHum: %i, maxHum: %i, webOR: %i, tempState: %i, humState: %i", minTemp, maxTemp, minHum, maxHum, webOR, tempState, humState);
		if ( tempState == 2 ) printf("\nTemp: %.1f C Range: (%i - %i) %.1f C from minTemp. ", temp, minTemp, maxTemp, temp - minTemp);
		else printf("\nTemp: %.1f C Range: (%i - %i) %.1f C from minTemp. ", temp, minTemp, maxTemp, minTemp - temp);
		// read temperature sensor
		CheckTemp();

		if ( humState == 2 ) printf("\nHumi: %.1f %% Range: (%i - %i) %.1f %% from minHumi. ", hum, minHum, maxHum, hum - minHum);
		else printf("\nHumi: %.1f %% Range: (%i - %i) %.1f %% from minHumi. ", hum, minHum, maxHum, minHum - hum);
		CheckHum();

		if (!webOR) { ChangeRelays ( maxTemp - temp, maxHum - hum ); }
		else { 
			printf("\nWebOverride activated - No relay change!");
			printf("\nWaiting 60 seconds.\n");
			sleep ( 60 );
		}
	}
	return 0;
}


int readCfgCond (void) {
	config_t cfg;
	config_init(&cfg);

	/* Read the file. If there is an error, report it and exit. */
	if (!config_read_file(&cfg, config_cond_path)) {
		printf("\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	/*  */
	if (!config_lookup_int(&cfg, "minTemp", &minTemp))  printf("\nNo 'minTemp' setting in configuration file.");
	if (!config_lookup_int(&cfg, "maxTemp", &maxTemp)) printf("\nNo 'maxTemp' setting in configuration file.");
	if (!config_lookup_int(&cfg, "minHum", &minHum)) printf("\nNo 'minHum' setting in configuration file.");
	if (!config_lookup_int(&cfg, "maxHum", &maxHum)) printf("\nNo 'maxHum' setting in configuration file.");
	if (!config_lookup_int(&cfg, "webOR", &webOR)) printf("\nNo 'webOR' setting in configuration file.");

	config_destroy(&cfg);
	return 0;
}

int readCfgState (void) {
	config_t cfg;
	config_init(&cfg);

	/* Read the file. If there is an error, report it and exit. */
	if (!config_read_file(&cfg, config_state_path)) {
		printf("\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	/*  */
	if (!config_lookup_int(&cfg, "tempState", &tempState)) printf("\nNo 'tempState' setting in configuration file.");
	if (!config_lookup_int(&cfg, "humState", &humState)) printf("\nNo 'humState' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay1o", &relay1o)) printf("\nNo 'relay1o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay2o", &relay2o)) printf("\nNo 'relay2o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay3o", &relay3o)) printf("\nNo 'relay3o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay4o", &relay4o)) printf("\nNo 'relay4o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay5o", &relay5o)) printf("\nNo 'relay5o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay6o", &relay6o)) printf("\nNo 'relay6o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay7o", &relay7o)) printf("\nNo 'relay7o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "relay8o", &relay8o)) printf("\nNo 'relay8o' setting in configuration file.");
	if (!config_lookup_int(&cfg, "RHeatTS", &RHeatTS)) printf("\nNo 'RHeatTS' setting in configuration file.");
	if (!config_lookup_int(&cfg, "RHumTS", &RHumTS)) printf("\nNo 'RHumTS' setting in configuration file.");
	if (!config_lookup_int(&cfg, "RHepaTS", &RHepaTS)) printf("\nNo 'RHepaTS' setting in configuration file.");
	if (!config_lookup_int(&cfg, "RFanTS", &RFanTS)) printf("\nNo 'RFanTS' setting in configuration file.");
	if (!config_lookup_float(&cfg, "wfactor", &wfactor)) printf("\nNo 'wfactor' setting in configuration file.");


	config_destroy(&cfg);
	return 0;
}

int writeCfgCond(void) {
	config_t cfg;
	config_setting_t *tminTemp = 0;
	config_setting_t *tmaxTemp = 0;
	config_setting_t *tminHum = 0;
	config_setting_t *tmaxHum = 0;
	config_setting_t *twebOR = 0;

	config_init(&cfg);

	if (!config_read_file(&cfg, config_cond_path))
	{
		printf("\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	/* lookup variables in config file */
	tminTemp = config_lookup(&cfg, "minTemp");
	tmaxTemp = config_lookup(&cfg, "maxTemp");
	tminHum = config_lookup(&cfg, "minHum");
	tmaxHum = config_lookup(&cfg, "maxHum");
	twebOR = config_lookup(&cfg, "webOR");

	/* get variables from config file then print the variables that changed */
	if (config_setting_get_int(tminTemp) != minTemp) {
		printf("\nminTemp: %i -> ", config_setting_get_int(tminTemp));
		config_setting_set_int(tminTemp, minTemp);
		printf("%i", config_setting_get_int(tminTemp));
	}
	if (config_setting_get_int(tmaxTemp) != maxTemp) {
		printf("\nmaxTemp: %i -> ", config_setting_get_int(tmaxTemp));
		config_setting_set_int(tmaxTemp, maxTemp);
		printf("%i", config_setting_get_int(tmaxTemp));
	}
	if (config_setting_get_int(tminHum) != minHum) {
		printf("\nminHum: %i -> ", config_setting_get_int(tminHum));
		config_setting_set_int(tminHum, minHum);
		printf("%i", config_setting_get_int(tminHum));
	}
	if (config_setting_get_int(tmaxHum) != maxHum) {
		printf("\nmaxHum: %i -> ", config_setting_get_int(tmaxHum));
		config_setting_set_int(tmaxHum, maxHum);
		printf("%i", config_setting_get_int(tmaxHum));
	}
	if (config_setting_get_int(twebOR) != webOR) {
		printf("\nwebOR: %i -> ", config_setting_get_int(twebOR));
		config_setting_set_int(twebOR, webOR);
		printf("%i", config_setting_get_int(twebOR));
	}

	/* write the modified config file */
	config_write_file(&cfg, config_cond_path);
	config_destroy(&cfg);
	return 0;
}

int writeCfgState(void) {
	config_t cfg;
	config_setting_t *ttempState = 0;
	config_setting_t *thumState = 0;
	config_setting_t *tRHeatTS = 0;
	config_setting_t *tRHumTS = 0;
	config_setting_t *tRHepaTS = 0;
	config_setting_t *tRFanTS = 0;
	config_setting_t *twfactor = 0;
	config_setting_t *trelay1o = 0;
	config_setting_t *trelay2o = 0;
	config_setting_t *trelay3o = 0;
	config_setting_t *trelay4o = 0;
	config_setting_t *trelay5o = 0;
	config_setting_t *trelay6o = 0;
	config_setting_t *trelay7o = 0;
	config_setting_t *trelay8o = 0;

	config_init(&cfg);

	if (!config_read_file(&cfg, config_state_path))
	{
		printf("\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	/* lookup variables in config file */
	ttempState = config_lookup(&cfg, "tempState");
	thumState = config_lookup(&cfg, "humState");
	trelay1o = config_lookup(&cfg, "relay1o");
	trelay2o = config_lookup(&cfg, "relay2o");
	trelay3o = config_lookup(&cfg, "relay3o");
	trelay4o = config_lookup(&cfg, "relay4o");
	trelay5o = config_lookup(&cfg, "relay5o");
	trelay6o = config_lookup(&cfg, "relay6o");
	trelay7o = config_lookup(&cfg, "relay7o");
	trelay8o = config_lookup(&cfg, "relay8o");
	tRHeatTS = config_lookup(&cfg, "RHeatTS");
	tRHumTS = config_lookup(&cfg, "RHumTS");
	tRHepaTS = config_lookup(&cfg, "RHepaTS");
	tRFanTS = config_lookup(&cfg, "RFanTS");
	twfactor = config_lookup(&cfg, "wfactor");

	/* get variables from config file then print the variables that changed */
	if (config_setting_get_int(ttempState) != tempState) {
		printf("\ntempState: %i -> ", config_setting_get_int(ttempState));
		config_setting_set_int(ttempState, tempState);
		printf("%i", config_setting_get_int(ttempState));
	}
	if (config_setting_get_int(thumState) != humState) {
		printf("\nhumState: %i -> ", config_setting_get_int(thumState));
		config_setting_set_int(thumState, humState);
		printf("%i", config_setting_get_int(thumState));
	}
	if (config_setting_get_int(trelay1o) != relay1o) {
		printf("\nrelay1o: %i -> ", config_setting_get_int(trelay1o));
		config_setting_set_int(trelay1o, relay1o);
		printf("%i", config_setting_get_int(trelay1o));
	}
	if (config_setting_get_int(trelay2o) != relay2o) {
		printf("\nrelay2o: %i -> ", config_setting_get_int(trelay2o));
		config_setting_set_int(trelay2o, relay2o);
		printf("%i", config_setting_get_int(trelay2o));
	}
	if (config_setting_get_int(trelay3o) != relay3o) {
		printf("\nrelay3o: %i -> ", config_setting_get_int(trelay3o));
		config_setting_set_int(trelay3o, relay3o);
		printf("%i", config_setting_get_int(trelay3o));
	}
	if (config_setting_get_int(trelay4o) != relay4o) {
		printf("\nrelay4o: %i -> ", config_setting_get_int(trelay4o));
		config_setting_set_int(trelay4o, relay4o);
		printf("%i", config_setting_get_int(trelay4o));
	}
	if (config_setting_get_int(trelay5o) != relay5o) {
		printf("\nrelay5o: %i -> ", config_setting_get_int(trelay5o));
		config_setting_set_int(trelay5o, relay5o);
		printf("%i", config_setting_get_int(trelay5o));
	}
	if (config_setting_get_int(trelay6o) != relay6o) {
		printf("\nrelay6o: %i -> ", config_setting_get_int(trelay6o));
		config_setting_set_int(trelay6o, relay6o);
		printf("%i", config_setting_get_int(trelay6o));
	}
	if (config_setting_get_int(trelay7o) != relay7o) {
		printf("\nrelay7o: %i -> ", config_setting_get_int(trelay7o));
		config_setting_set_int(trelay7o, relay7o);
		printf("%i", config_setting_get_int(trelay7o));
	}
	if (config_setting_get_int(trelay8o) != relay8o) {
		printf("\nrelay8o: %i -> ", config_setting_get_int(trelay8o));
		config_setting_set_int(trelay8o, relay8o);
		printf("%i", config_setting_get_int(trelay8o));
	}
	if (config_setting_get_int(tRHeatTS) != RHeatTS) {
		printf("\nRHeatTS: %i -> ", config_setting_get_int(tRHeatTS));
		config_setting_set_int(tRHeatTS, RHeatTS);
		printf("%i", config_setting_get_int(tRHeatTS));
	}
	if (config_setting_get_int(tRHumTS) != RHumTS) {
		printf("\nRHumTS: %i -> ", config_setting_get_int(tRHumTS));
		config_setting_set_int(tRHumTS, RHumTS);
		printf("%i", config_setting_get_int(tRHumTS));
	}
	if (config_setting_get_int(tRHepaTS) != RHepaTS) {
		printf("\nRHepaTS: %i -> ", config_setting_get_int(tRHepaTS));
		config_setting_set_int(tRHepaTS, RHepaTS);
		printf("%i", config_setting_get_int(tRHepaTS));
	}
	if (config_setting_get_int(tRFanTS) != RFanTS) {
		printf("\nRFanTS: %i -> ", config_setting_get_int(tRFanTS));
		config_setting_set_int(tRFanTS, RFanTS);
		printf("%i", config_setting_get_int(tRFanTS));
	}
	if (config_setting_get_float(twfactor) != wfactor) {
		printf("\nwfactor: %i -> ", config_setting_get_float(twfactor));
		config_setting_set_float(twfactor, wfactor);
		printf("%i", config_setting_get_float(twfactor));
	}

	/* write the modified config file */
	config_write_file(&cfg, config_state_path);
	config_destroy(&cfg);
	return 0;
}

int CheckTemp (void) {
	if ( temp < minTemp ) {
		tempState = 1;
		printf("< minTemp. Heat On.");
	} else if ( temp >= minTemp && temp < maxTemp) {
		tempState = 2;
		printf(">= minTemp. Heat off until < minTemp.");
	} else if ( temp >= maxTemp ) {
		tempState = 3;
		printf("> maxTemp. Heat Off, All fans On.");
	} else {
		if ( tempState == 2 ) printf("Heat off until < minTemp.");
		else printf("Heat on until > minTemp.");
		return;
	}
	writeCfgState();	
	return 0;
}

int CheckHum (void) {
	if ( hum < minHum ) {
		humState = 1;
		printf("< minHum. Hum On.");
	} else if ( hum >= minHum) {
		humState = 2;
		printf(">= minHum. Hum Off until < minHum.");
	} else {
		if ( humState == 2 ) printf("Leaving off until < minHum.");
		else printf("Leaving on until > minHum.");
		return 0;
	}
	writeCfgState();
	return 0;
}

relay1(int i) { // HEPA Fan
	printf("\nRelay 1 (HEPA): ");
	if (relay1o) {
		char com[120];
		switch (i) {
			case 0:
				printf("Off");
				sprintf(com, "%s 1 0 &", relay_exec_path);
				system(com);
				break;
			case 1:
				if (humState == 3 && tempState != 3) {
					printf("Turn on for %d seconds, then off for %d seconds.", RHepat[RHepat[0]], RHepao[RHepat[0]]);
					sprintf(com, "%s %d %d &", relay_exec_path, RHepa, RHepat[RHepat[0]]);
					system(com);
					if (RHepao[RHepat[0]] + RHepat[RHepat[0]] < sleept) {
						sleept = RHepao[RHepat[0]] + RHepat[RHepat[0]];
					}
					RHepaTS = timestampL;
					writeCfgState();
				} else {
					printf("Turn on until temperature < maxTemp or humidity < maxHum.");
					sprintf(com, "%s 1 1 &", relay_exec_path);
					system(com);
				}
				break;
			case 2:
				printf("Remaining on for %d more seconds (of %d)", RHepat[RHepat[0]] - (timestampL - RHepaTS), RHepat[RHepat[0]]);
				if (RHepat[RHepat[0]] - (timestampL - RHepaTS) < sleept) {
					sleept = RHepat[RHepat[0]] - (timestampL - RHepaTS);
				}
				break;
			case 3:
				printf("Off for %d more seconds.", RHepao[RHepat[0]] - (timestampL - RHepaTS));
				sprintf(com, "%s 1 0 &", relay_exec_path);
				system(com);
				if (RHepao[RHepat[0]] - (timestampL - RHepaTS) < sleept) {
					sleept = RHepao[RHepat[0]] - (timestampL - RHepaTS);
				}
				break;
		}
	} else printf("Override");
}

relay2(int i) { // Humidifier
	printf("\nRelay 2 (HUMI): ");
	if (relay2o) {
		char com[120];
		switch (i) {
			case 0: // Turn relay2 off
				printf("Off until humidity < minHum");
				sprintf(com, "%s 2 0 &", relay_exec_path);
				system(com);
				break;
			case 1:
				RHumo[0] = RHumt[0];
				printf("Turn on for %d sec, then off for %d seconds.", RHumt[RHumt[0]], RHumo[RHumt[0]]);
				sprintf(com, "%s %d %d &", relay_exec_path, RHum, RHumt[RHumt[0]]);
				system(com);
				if (RHumo[RHumt[0]] + RHumt[RHumt[0]] < sleept) {
					sleept = RHumo[RHumt[0]] + RHumt[RHumt[0]];
				}
				RHumTS = timestampL;
				writeCfgState();
				break;
			case 2:
				printf("Remaining on for %d more seconds (of %d)", RHumt[RHumt[0]] - (timestampL - RHumTS), RHumt[RHumt[0]]);
				if ( RHumt[RHumt[0]] - (timestampL - RHumTS) < sleept) {
					sleept = RHumt[RHumt[0]] - (timestampL - RHumTS);
				}
				break;
			case 3:
				printf("Off for %d more seconds.", RHumo[RHumt[0]] + RHumt[RHumt[0]] - (timestampL - RHumTS));
				sprintf(com, "%s 2 0 &", relay_exec_path);
				system(com);
				if (RHumo[RHumt[0]] + RHumt[RHumt[0]] - (timestampL - RHumTS) < sleept) {
					sleept = RHumo[RHumt[0]] + RHumt[RHumt[0]] - (timestampL - RHumTS);
				}
				break;
		}
	} else printf("Override");
}

relay3(int i) { // Circulatory Fan
	printf("\nRelay 3 (CFAN): ");
	if (relay3o) {
		char com[120];
		switch (i) {
			case 0:
				printf("Off until temperature > maxTemp or humidity > maxHum.");
				sprintf(com, "%s 3 0 &", relay_exec_path);
				system(com);
				break;
			case 1:
				printf("Turn on for %d sec, then off for %d seconds.", RFant[RFant[0]], RFano[RFant[0]]);
				sprintf(com, "%s %d %d &", relay_exec_path, RFan, RFant[RFant[0]]);
				system(com);
				if (RFano[RFant[0]] + RFant[RFant[0]] < sleept) {
					sleept = RFano[RFant[0]] + RFant[RFant[0]];
				}
				RFanTS = timestampL;
				writeCfgState();
				break;
			case 2: // Leave on
				printf("Remaining on for %d more seconds (of %d)", RFant[RFant[0]] - (timestampL - RFanTS), RFant[RFant[0]]);
				if ( RFant[RFant[0]] - (timestampL - RFanTS) < sleept ) {
					sleept = RFant[RFant[0]] - (timestampL - RFanTS);
				}
				break;
			case 3:
				printf("Off for %d more seconds.", RFano[RFant[0]] + RFant[RFant[0]] - (timestampL - RFanTS));
				sprintf(com, "%s 3 0 &", relay_exec_path);
				system(com);
				if (RFano[RFant[0]] + RFant[RFant[0]] - (timestampL - RFanTS) < sleept) {
					sleept = RFano[RFant[0]] + RFant[RFant[0]] - (timestampL - RFanTS);
				}
				break;
		}
	} else printf("Override");
}

relay4(int i) { // Heat
	printf("\nRelay 4 (HEAT): ");
	if (relay4o) {
		char com[120];
		switch (i) {
			case 0: // Turn off
				printf("Off until temperature < minTemp.");
				sprintf(com, "%s 4 0 &", relay_exec_path);
				system(com);
				break;
			case 1: // Turn on
				RHeato[0] = RHeatt[0];
				printf("Turn on for %d sec, then off for %d seconds.", RHeatt[RHeatt[0]], (int)(RHeato[RHeatt[0]]*wfactor));
				sprintf(com, "%s %d %d &", relay_exec_path, RHeat, RHeatt[RHeatt[0]]);
				system(com);
				if((int)(RHeato[RHeatt[0]]*wfactor) + (int)(RHeatt[RHeatt[0]]*wfactor)< sleept) {
					sleept = (int)(RHeato[RHeatt[0]]*wfactor) + (int)(RHeatt[RHeatt[0]]*wfactor);
				}
				RHeatTS = timestampL;
				writeCfgState();
				break;
			case 2: // Leave on
				printf("Remaining on for %d more seconds (of %d)", RHeatt[RHeatt[0]] - (timestampL - RHeatTS), RHeatt[RHeatt[0]]);
				if ( RHeatt[RHeatt[0]] - (timestampL - RHeatTS) < sleept ) {
					sleept = RHeatt[RHeatt[0]] - (timestampL - RHeatTS);
				}
				break;
			case 3:
				printf("Off for %d more seconds.", (int)(RHeato[RHeatt[0]]*wfactor) + RHeatt[RHeatt[0]] - (timestampL - RHeatTS));
				sprintf(com, "%s 4 0 &", relay_exec_path);
				system(com);
				if ((int)(RHeato[RHeatt[0]]*wfactor) + RHeatt[RHeatt[0]] - (timestampL - RHeatTS) < sleept) {
					sleept = (int)(RHeato[RHeatt[0]]*wfactor) + RHeatt[RHeatt[0]] - (timestampL - RHeatTS);
				}
				break;
		}
	} else printf("Override");
}

int ChangeRelays (double tdiff, double hdiff) {
	if ( tdiff > RFans[0] ) RFant[0] = 1;
	else if ( tdiff > RFans[1] ) RFant[0] = 2;
	else if ( tdiff > RFans[2] ) RFant[0] = 3;
	else if ( tdiff > RFans[3] ) RFant[0] = 4;
	else RFant[0] = 5;

	if ( hdiff > RFans[0] ) RFant[0] = 1;
	else if ( hdiff > RFans[1] ) RFant[0] = 2;
	else if ( hdiff > RFans[2] ) RFant[0] = 3;
	else if ( hdiff > RFans[3] ) RFant[0] = 4;
	else RFant[0] = 5;

	if ( tdiff > RFans[0] ) RHepat[0] = 1;
	else if ( tdiff > RFans[1] ) RHepat[0] = 2;
	else if ( tdiff > RFans[2] ) RHepat[0] = 3;
	else if ( tdiff > RFans[3] ) RHepat[0] = 4;
	else RHepat[0] = 5;

	if ( tdiff < RHeats[0] ) RHeatt[0] = 1;
	else if ( tdiff < RHeats[1] ) RHeatt[0] = 2;
	else if ( tdiff < RHeats[2] ) RHeatt[0] = 3;
	else if ( tdiff < RHeats[3] ) RHeatt[0] = 4;
	else RHeatt[0] = 5;

	if ( hdiff < RHums[0] ) RHumt[0] = 1;
	else if ( hdiff < RHums[1] ) RHumt[0] = 2;
	else if ( hdiff < RHums[2] ) RHumt[0] = 3;
	else if ( hdiff < RHums[3] ) RHumt[0] = 4;
	else RHumt[0] = 5;

	// tempState: 1=below minTemp, 2=above maxTemp now cooling, 3=too high turn all fans on
	// humState:  1=below minHum,  2=above maxHum now cooling
	// Relays: RHepa 1, RHum  2, RFan  3, RHeat 4
	// relayx(Y), Y: 0=Turn off, 1=Turn on, 2=Leave on, 3=Leave off 

	if ( tempState == 1 && humState == 1 ) {// Heat on, humidifier on
		// Relay 1 HEPA
		relay1(0); // Turn relay1 off
		usleep(500000);

		// Relay 2 Humidifier
		if (timestampL - RHumTS - RHumt[RHumt[0]] < RHumo[RHumt[0]]) { // If not end of off timer
			if (timestampL - RHumTS < RHumt[RHumt[0]]) { // If not at end of on timer
				relay2(2); // Leave relay2 on
			} else {
				if (RHumo[RHumt[0]] + RHumt[RHumt[0]] - (timestampL - RHumTS) <= 0) {
					relay2(1); // Turn relay2 on
				}
				else {
					relay2(3); // Leave relay2 off
				}
			}
		} else {
			relay2(1); // Turn relay2 on
		}
		usleep(500000);

		// Relay 3 Circulatory Fan
		relay3(0); // Turn relay3 off
		usleep(500000);

		// Relay 4 Heater
		if (timestampL - RHeatTS - RHeatt[RHeatt[0]] < (int)(RHeato[RHeatt[0]]*wfactor)) { // If not end of on and off timer
			if (timestampL - RHeatTS < RHeatt[RHeatt[0]]) { // If not at end of on timer
				relay4(2); // Leave relay4 on
			} else {
				if ((int)(RHeato[RHeatt[0]]*wfactor) + RHeatt[RHeatt[0]] - (timestampL - RHeatTS) <= 0) relay4(1); // Turn relay4 on
				else relay4(3); // Leave relay4 off
			}
		} else relay4(1); // Turn relay4 on
	}
	else if ( tempState == 2 && humState == 1 )
	{
		// Relay 1 HEPA
		relay1(0); // Turn relay1 off
		usleep(500000);

		// Relay 2 Humidifier
		if (timestampL - RHumTS - RHumt[RHumt[0]] < RHumo[RHumt[0]]) { // If not end of off timer
			if (timestampL - RHumTS < RHumt[RHumt[0]]) { // If not at end of on timer
				relay2(2); // Leave relay2 on
			} else {
				if (RHumo[RHumt[0]] + RHumt[RHumt[0]] - (timestampL - RHumTS) <= 0) {
					relay2(1); // Turn relay2 on
				}
				else {
					relay2(3); // Leave relay2 off
				}
			}
		} else {
			relay2(1); // Turn relay2 on
		}
		usleep(500000);

		// Relay 3 Circulatory Fan
		relay3(0); // Turn relay3 off
		usleep(500000);

		// Relay 4 Heater
		relay4(0); // Turn relay4 off until minTemp reached
	}
	else if ( tempState == 3 && humState == 1 )
	{
		// Relay 1 HEPA
		relay1(1); // Turn relay1 on
		usleep(500000);

		// Relay 2 Humidifier, Relay 3 Circulatory Fan
		if (timestampL - RHumTS - RHumt[RHumt[0]] < RHumo[RHumt[0]]) { // If not end of off timer
			if (timestampL - RHumTS < RHumt[RHumt[0]]) { // If not at end of on timer
				relay2(2); // Leave relay2 on
			} else {
				if (RHumo[RHumt[0]] + RHumt[RHumt[0]] - (timestampL - RHumTS) <= 0) {
					relay2(1); // Turn relay2 on
				}
				else {
					relay2(3); // Leave relay2 off
				}
			}
		} else {
			relay2(1); // Turn relay2 on
		}
		usleep(500000);

		// Relay 3 Circulatory Fan
		if (timestampL - RFanTS < RFano[RFant[0]]) { // If not end of off timer
			if (timestampL - RFanTS < RFant[RFant[0]]) { // If not at end of on timer
				relay3(2); // Leave relay1 on
			} else relay3(3); // Leave relay1 off
		} else relay3(1); // Turn relay1 on
		usleep(500000);

		// Relay 4 Heater
		relay4(0); // Turn relay4 off until minTemp reached
	}
	else if ( tempState == 3 && (humState == 2 || humState == 3))
	{
		// Relay 1 HEPA
		relay1(1); // Turn relay1 on
		usleep(500000);

		// Relay 2 Humidifier
		relay2(0); // Turn relay2 off
		usleep(500000);

		// Relay 3 Circulatory Fan
		if (timestampL - RFanTS < RFano[RFant[0]]) { // If not end of off timer
			if (timestampL - RFanTS < RFant[RFant[0]]) { // If not at end of on timer
				relay3(2); // Leave relay1 on
			} else relay3(3); // Leave relay1 off
		} else relay3(1); // Turn relay1 on
		usleep(500000);

		// Relay 4 Heat
		relay4(0); // Turn relay4 off
	}
	else if ( tempState == 1 && (humState == 2 || humState == 3) )
	{
		// Relay 1 HEPA
		if ( humState == 3 ) {
			if (timestampL - RHepaTS < RHepao[RHepat[0]]) { // If not end of off timer
				if (timestampL - RHepaTS < RHepat[RHepat[0]]) { // If not at end of on timer
					relay1(2); // Leave relay1 on
				} else relay1(3); // Leave relay1 off
			} else relay1(1); // Turn relay1 on
		} else relay1(0); // Turn relay1 off

		relay1(0); // Turn relay1 off
		usleep(500000);

		// Relay 2 Humidifier
		relay2(0); // Turn relay2 off
		usleep(500000);

		// Relay 3 Circulatory Fan
		if (humState == 3) {
			if (timestampL - RFanTS < RFano[RFant[0]]) { // If not end of off timer
				if (timestampL - RFanTS < RFant[RFant[0]]) { // If not at end of on timer
					relay3(2); // Leave relay1 on
				} else relay3(3); // Leave relay1 off
			} else relay3(1); // Turn relay1 on
		} else relay3(0); // Turn relay3 off
		usleep(500000);

		// Relay 4 Heater
		if (timestampL - RHeatTS - RHeatt[RHeatt[0]] < (int)(RHeato[RHeatt[0]]*wfactor)) { // If not end of on and off timer
			if (timestampL - RHeatTS < RHeatt[RHeatt[0]]) { // If not at end of on timer
				relay4(2); // Leave relay4 on
			} else {
				if ((int)(RHeato[RHeatt[0]]*wfactor) + RHeatt[RHeatt[0]] - (timestampL - RHeatTS) <= 0) relay4(1); // Turn relay4 on
				else relay4(3); // Leave relay4 off
			}
		} else relay4(1); // Turn relay4 on
	}
	else if (tempState == 2 && (humState == 2 || humState == 3))
	{
		// Relay 1 HEPA
		if ( humState == 3 ) {
			if (timestampL - RHepaTS < RHepao[RHepat[0]]) { // If not end of off timer
				if (timestampL - RHepaTS < RHepat[RHepat[0]]) { // If not at end of on timer
					relay1(2); // Leave relay1 on
				} else relay1(3); // Leave relay1 off
			} else relay1(1); // Turn relay1 on
		} else relay1(0); // Turn relay1 off
		usleep(500000);

		// Relay 2 Humidifier
		relay2(0); // Turn relay2 off
		usleep(500000);

		// Relay 3 Circulatory Fan
		if (humState == 3) {
			if (timestampL - RFanTS < RFano[RFant[0]]) { // If not end of off timer
				if (timestampL - RFanTS < RFant[RFant[0]]) { // If not at end of on timer
					relay3(2); // Leave relay1 on
				} else relay3(3); // Leave relay1 off
			} else relay3(1); // Turn relay1 on
		} else relay3(0); // Turn relay3 off
		usleep(500000);

		// Relay 4 Heater
		relay4(0); // Turn relay4 off
	}
	else printf("Something went wrong!\n");

	if (sleept <= 0) {
		printf("\nsleept <= 0 (review for errors in code!). Setting to 60.");
		sleep( 60 );
	} else {
		printf("\nRefresh in %d seconds.\n", sleept);
		sleep( sleept );
	}

	return 0;
}

