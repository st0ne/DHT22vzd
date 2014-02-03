/**************************************************************************

DHT22 to Volkszaehler 'RaspberryPI deamon'.

https://github.com/st0ne/DHT22vzd

Philipp Aigner  <phaigner@gmail.com>

**************************************************************************/

#define DAEMON_NAME "DHT22vzd"
#define DAEMON_VERSION "0.1"
#define DAEMON_BUILD "1"

// DHT22 pin configuration
#define MAXTIMINGS 85
#define DHTPIN 7

/**************************************************************************

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

**************************************************************************/

#include <wiringPi.h>

#include <stdio.h>              /* standard library functions for file input and output */
#include <stdlib.h>             /* standard library for the C programming language, */
#include <string.h>             /* functions implementing operations on strings  */
#include <unistd.h>             /* provides access to the POSIX operating system API */
#include <sys/stat.h>           /* declares the stat() functions; umask */
#include <fcntl.h>              /* file descriptors */
#include <syslog.h>             /* send messages to the system logger */
#include <errno.h>              /* macros to report error conditions through error codes */
#include <signal.h>             /* signal processing */
#include <stddef.h>             /* defines the macros NULL and offsetof as well as the types ptrdiff_t, wchar_t, and size_t */
#include <dirent.h>		/* constructs that facilitate directory traversing */

#include <libconfig.h>          /* reading, manipulating, and writing structured configuration files */
#include <curl/curl.h>          /* multiprotocol file transfer library */

void daemonShutdown();
void signal_handler(int sig);
void daemonize(char *rundir, char *pidfile);

int pidFilehandle, minterval, vzport, i, count;

const char *vzserver, *vzpath, *uuid;


static int dht22_dat[5] = {0,0,0,0,0};

char url[128];

static float humidity;
static float temperature;

char temperature_uuid[64];
char humidity_uuid[64];

config_t cfg;

void signal_handler(int sig) {
	switch(sig)
	{
		case SIGHUP:
		syslog(LOG_WARNING, "Received SIGHUP signal.");
		break;
		case SIGINT:
		case SIGTERM:
		syslog(LOG_INFO, "Daemon exiting");
		daemonShutdown();
		exit(EXIT_SUCCESS);
		break;
		default:
		syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
		break;
	}
}

void  daemonShutdown() {
		close(pidFilehandle);
		remove("/tmp/1wirevz.pid");
}

void daemonize(char *rundir, char *pidfile) {
	int pid, sid, i;
	char str[10];
	struct sigaction newSigAction;
	sigset_t newSigSet;

	if (getppid() == 1)
	{
		return;
	}

	sigemptyset(&newSigSet);
	sigaddset(&newSigSet, SIGCHLD);
	sigaddset(&newSigSet, SIGTSTP);
	sigaddset(&newSigSet, SIGTTOU);
	sigaddset(&newSigSet, SIGTTIN);
	sigprocmask(SIG_BLOCK, &newSigSet, NULL);

	newSigAction.sa_handler = signal_handler;
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;

	sigaction(SIGHUP, &newSigAction, NULL);
	sigaction(SIGTERM, &newSigAction, NULL);
	sigaction(SIGINT, &newSigAction, NULL);

	pid = fork();

	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}

	if (pid > 0)
	{
		printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}
	
	//umask(027);

	sid = setsid();
	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	for (i = getdtablesize(); i >= 0; --i)
	{
		close(i);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	chdir(rundir);

	pidFilehandle = open(pidfile, O_RDWR|O_CREAT, 0600);

	if (pidFilehandle == -1 )
	{
		syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	if (lockf(pidFilehandle,F_TLOCK,0) == -1)
	{
		syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	sprintf(str,"%d\n",getpid());

	write(pidFilehandle, str, strlen(str));
}

void cfile() {

	const char *uuid;
	config_init(&cfg);

	int chdir(const char *path);

	chdir ("/etc");

	if(!config_read_file(&cfg, DAEMON_NAME".cfg"))
	{
		syslog(LOG_INFO, "Config error > /etc/%s - %s\n", config_error_file(&cfg),config_error_text(&cfg));
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}

	if (!config_lookup_string(&cfg, "vzserver", &vzserver))
	{
		syslog(LOG_INFO, "Missing 'VzServer' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzServer: %s", vzserver);

	if (!config_lookup_int(&cfg, "vzport", &vzport))
	{
		syslog(LOG_INFO, "Missing 'VzPort' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzPort: %d", vzport);


	if (!config_lookup_string(&cfg, "vzpath", &vzpath))
	{
		syslog(LOG_INFO, "Missing 'VzPath' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzPath: %s", vzpath);

	if (!config_lookup_int(&cfg, "interval", &minterval))
	{
		syslog(LOG_INFO, "Missing 'Interval' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "Metering interval: %d sec", minterval);

	if (!config_lookup_string(&cfg, "humidityuuid", &uuid))
    {
        syslog(LOG_INFO, "Missing 'Humidity UUID' setting in configuration file.");
        config_destroy(&cfg);
        daemonShutdown();
        exit(EXIT_FAILURE);
    }
    else
	{
		strcpy(humidity_uuid, uuid);
    	syslog(LOG_INFO, "Humidity UUID: %s", humidity_uuid);
	}

 	if (!config_lookup_string(&cfg, "temperatureuuid", &uuid))
    {
        syslog(LOG_INFO, "Missing 'Temperature UUID' setting in configuration file.");
        config_destroy(&cfg);
        daemonShutdown();
        exit(EXIT_FAILURE);
    }
    else
	{
		strcpy(temperature_uuid, uuid);
    	syslog(LOG_INFO, "Temperature UUID: %s", temperature_uuid);
	}

}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
< 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    syslog(LOG_INFO,"Invalid data from wiringPi library"); 
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

static int read_dht22_dat()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40);
  // prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 16)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) &&
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0) t *= -1;


    syslog(LOG_INFO,"Humidity = %.2f %% Temperature = %.2f *C", h, t );
	humidity = h;
	temperature = t;
    return 1;
  }
  else
  {
    return 0;
  }
}


void http_post( float value, char  *vzuuid ) {

	CURL *curl;
	CURLcode curl_res;
 
	sprintf ( url, "http://%s:%d/%s/data/%s.json?value=%.2f", vzserver, vzport, vzpath, vzuuid, value );

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	if(curl)
	{ 

		FILE* devnull = NULL;
		devnull = fopen("/dev/null", "w+");

		curl_easy_setopt(curl, CURLOPT_USERAGENT, DAEMON_NAME " " DAEMON_VERSION ); 
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
		
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
	
		if( (curl_res  = curl_easy_perform(curl)) != CURLE_OK) {
		syslog ( LOG_INFO, "HTTP_POST(): %s", curl_easy_strerror(curl_res) );
		}
	
		curl_easy_cleanup(curl);
		fclose ( devnull );
	}

curl_global_cleanup();
}

int main() { 

	syslog(LOG_INFO,"Raspberry Pi wiringPi DHT22 reader daemon based on lol_dht22 and 1wirevz");

	// init wiringPI
	if (wiringPiSetup () == -1)
    exit(EXIT_FAILURE) ;

	if (setuid(getuid()) < 0)
  	{
    	perror("Dropping privileges failed\n");
    	exit(EXIT_FAILURE);
  	}

	// read values from the config file
	cfile();	

	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

//	syslog(LOG_INFO, "DS2482 I²C 1-Wire® Master to Volkszaehler deamon %s (%s) %d", DAEMON_VERSION, DAEMON_BUILD, count_i2cdevices());

	char pid_file[16];
	sprintf ( pid_file, "/tmp/%s.pid", DAEMON_NAME );
	daemonize( "/tmp/", pid_file );
				
	while(1) {
	
		// reading the sensor until a valid value	
		while (read_dht22_dat() == 0)
		{
			syslog(LOG_INFO,"No data from sensor, retrying...");
			sleep(1);
		}
	   
		// posting the measured values to vz 
		http_post(temperature, temperature_uuid);
		http_post(humidity, humidity_uuid);	
	
		// sleep for the configured seconds	
		sleep(minterval);
	}
	
return(0);
}
