#include <stdio.h>

/*
Meaning of the leds


 VOIP : off - no account active/registered
        on - at least one account configured and registered
        
 PHONEx: off - on hook
         on - off hook, no call active
         slow blinking - off hook, call active        
         fast blinking - ringing
*/

static FILE *led_open(char *led, char* sub)
{
   char fname[100];
   
   snprintf(fname, sizeof(fname), "/sys/class/leds/%s/%s", led, sub);
   return fopen(fname, "rb+");
   
}

static FILE *led_trigger(char *led)
{
  return led_open(led, "trigger");
}

static void led_delay(char *led, int onoff, int msec)
{
  FILE *fp = led_open(led, onoff ? "delay_on" : "delay_off");
  if (fp) {
    fprintf(fp,"%d\n",msec);
    fclose(fp);
  }
}

void led_on(char *led)
{

 FILE *fp;
 
 fp = led_trigger(led);
 if (fp) {
   fprintf(fp,"default-on\n");
   fclose(fp);
 }
}

void led_off(char *led)
{

 FILE *fp;
 
 fp = led_trigger(led);
 if (fp) {
   fprintf(fp,"none\n");
   fclose(fp);
 }
}

void led_blink(char *led, int period)
{

 FILE *fp;
 
 fp = led_trigger(led);
 if (fp) {
   fprintf(fp, "timer\n");
   fclose(fp);
   led_delay(led, 1, period/2);
   led_delay(led, 0, period/2);
 }
}
