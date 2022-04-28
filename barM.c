/*
 * Copyright (C) 2014,2015 levi0x0 with enhancements by ProgrammerNerd
 * 
 * barM (bar_monitor or BarMonitor) is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 *  This is a new version of bar monitor, even less lines of code more effective.
 *
 *  Read main() to configure your new status Bar.
 *
 *  compile: gcc -o barM barM.c -O2 -s -lX11
 *  
 *  mv barM /usr/local/bin/
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <alsa/asoundlib.h>

#define LOOP_TIMEOUT 2000 //ms
#define VOLUME_BOUND 100
#define TIME_FORMAT "%Y年%m月%d日 %H:%M"
#define MAXSTR  1024
#define SYS_MEM_NAME_LEN 20
#define SYS_MEM_BUFF_LEN 256

/*
 *  Put this in your .xinitrc file: 
 *
 *  barM&
 *  
 */
typedef struct cpu_occupy_s {
    char name[20];
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
    unsigned int iowait;
    unsigned int irq;
    unsigned int softirq;
} cpu_occupy_t;

static const char * bright(void);
static const char * voice(void);
static const char * date(void);
static const char * cpu(void);
static const char * temp(void);
static const char * ram(void);
static void XSetRoot(const char *name);

cpu_occupy_t cpu_stat1;
snd_mixer_t *mixer;
int controls_changed = 0;
int control_values_changed = 0;
long outvol = 0;

/*Append here your functions.*/
static const char*(*const functab[])(void)={
        cpu, temp, ram, voice, bright, date
};

static int elem_callback(snd_mixer_elem_t *elem, unsigned int mask)
{
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
                controls_changed = 1;
        } else {
                if (mask & SND_CTL_EVENT_MASK_VALUE)
                        control_values_changed = 1;

		/*
                if (mask & SND_CTL_EVENT_MASK_INFO)
                        controls_changed = 1;
		*/
        }

        return 0;
}

static int mixer_callback(snd_mixer_t *mixer, unsigned int mask, snd_mixer_elem_t *elem){
        if (mask & SND_CTL_EVENT_MASK_ADD) {
                snd_mixer_elem_set_callback(elem, elem_callback);
        }
        return 0;
}

int init_mixer_handle(void)
{
        int err;
	char *card = "default";

        err = snd_mixer_open(&mixer, 0);
        if (err < 0){
                fprintf(stderr, "cannot open mixer\n");
		return err;
	}

    	if ((snd_mixer_attach(mixer, card)) < 0) {
		fprintf(stderr, "cannot attach mixer\n");
        	return err;
    	}

        err = snd_mixer_selem_register(mixer, NULL, NULL);
        if (err < 0) {
                fprintf(stderr, "cannot open mixer\n");
		return err;
	}

        snd_mixer_set_callback(mixer, mixer_callback);

        err = snd_mixer_load(mixer);
        if (err < 0) {
                fprintf(stderr, "cannot load mixer controls");
		return err;
	}

	return 0;
}


void update_vol(void)
{   
    long minv, maxv;
    snd_mixer_elem_t *elem; 

    elem = snd_mixer_first_elem(mixer);
    if (!elem) {
	outvol = 200;
	fprintf(stderr, "find elem error\n");
	return;
    }

    snd_mixer_selem_get_playback_volume_range (elem, &minv, &maxv);

    if(snd_mixer_selem_get_playback_volume(elem, 0, &outvol) < 0) {
	    outvol = 200;
	    fprintf(stderr, "get volume error\n");
    }

    /* make the value bound to 100 */
    outvol -= minv;
    maxv -= minv;

    outvol = (int)(100 * (float)(outvol) / maxv + 0.5); // make the value bound from 0 to 100
}

/* Returns voice size*/
static const char *voice(void)
{
    static char voice[MAXSTR];
    if (outvol == 200){
    	snprintf(voice, MAXSTR, " 音量：ERROR");
    }else{
	snprintf(voice, MAXSTR, " 音量：%ld%%", outvol);
    }

    return voice;
}

static double read_proc(char *proc_path){ 
	FILE *fd;
	double read_data;
	char buff[32];

	fd = fopen(proc_path, "r");
	if(fd == NULL)
        {
		return -1;
        }

	if(NULL == fgets(buff, sizeof(buff), fd)){
		return -1;
	}

	sscanf(buff, "%lf\n", &read_data);
	fclose(fd);

	return read_data;
}

/* Returns a string that contains the amount of free and available ram in megabytes*/
static const char * ram(void){
	FILE *fd;
	int i;
        static char ram[MAXSTR];
	char buff[SYS_MEM_BUFF_LEN];
	char name01[SYS_MEM_NAME_LEN];
	char name02[SYS_MEM_NAME_LEN];
	unsigned long mem[6];

	fd = fopen ("/proc/meminfo", "r");

	for(i=0; i<24; i++){
		fgets(buff, sizeof(buff), fd);

		if(i < 5){
			sscanf(buff, "%s %lu %s\n", name01, &mem[i], name02);
			mem[i] /= 1024;
		}
	}

	sscanf(buff, "%s %lu %s\n", name01, &mem[5], name02);
	mem[5] /= 1024;

	fclose(fd);

        snprintf(ram,sizeof(ram),"内存：%luM/%luM", mem[0]-mem[1]-mem[3]-mem[4]-mem[5], mem[0]);


	return ram;
}

/* Returns cpu usage */
double cal_cpuoccupy (cpu_occupy_t *o, cpu_occupy_t *n)
{
    double od, nd;
    double id, sd;
    double cpu_use ;
 
    od = (double) (o->user + o->nice + o->system + o->idle + o->softirq + o->iowait + o->irq);    
    nd = (double) (n->user + n->nice + n->system + n->idle + n->softirq + n->iowait + n->irq); 
    id = (double) (n->idle);
    sd = (double) (o->idle);
    if((nd-od) != 0)
        cpu_use =100.0 - ((id-sd))/(nd-od)*100.00;
    else 
        cpu_use = 0;
    return cpu_use;
}
 
void get_cpuoccupy (cpu_occupy_t *cpust)
{
    FILE *fd;
    char buff[256];
    cpu_occupy_t *cpu_occupy;
    cpu_occupy=cpust;

    fd = fopen ("/proc/stat", "r");
    if(fd == NULL)
    {
            perror("fopen:");
            exit (0);
    }
    fgets (buff, sizeof(buff), fd);

    sscanf (buff, "%s %u %u %u %u %u %u %u", cpu_occupy->name, &cpu_occupy->user, &cpu_occupy->nice,&cpu_occupy->system, &cpu_occupy->idle ,&cpu_occupy->iowait,&cpu_occupy->irq,&cpu_occupy->softirq);
 
    fclose(fd);
}

static const char * cpu(void) {
    	double cpu_usage;
	static char cpu[MAXSTR];
	cpu_occupy_t cpu_stat2;
    
	get_cpuoccupy((cpu_occupy_t *)&cpu_stat2);
 
    	cpu_usage = cal_cpuoccupy ((cpu_occupy_t *)&cpu_stat1, (cpu_occupy_t *)&cpu_stat2);
	cpu_stat1 = cpu_stat2;

	snprintf(cpu, MAXSTR, " CPU: %.2lf%%", cpu_usage);
	return cpu;
}

// Return cpu temp
static const char * temp(void) {
	unsigned long t;
	static char temp[MAXSTR];

	//fd = fopen ("/sys/class/hwmon/hwmon5/temp1_input", "r");
	t = read_proc("/sys/class/hwmon/hwmon5/temp1_input");
	t /= 1000.;

	snprintf(temp, MAXSTR, " 温度：%lu°C", t);
	return temp;
}
/* Returns monitor bright */

static const char * bright(void){
	double max_brightness, brightness;
	double b;
	static char bright[MAXSTR];

	max_brightness = read_proc("/sys/class/backlight/intel_backlight/max_brightness");
	brightness = read_proc("/sys/class/backlight/intel_backlight/brightness");

	b = (brightness/max_brightness) * 100.00;
	if(max_brightness <= 0) {
		snprintf(bright, MAXSTR, " 亮度：N/A");
	}else{
		snprintf(bright, MAXSTR, " 亮度：%.0lf%%", b);
	}

	return bright;
}

/* Returns the date*/
static const char * date(void){
        static char date[MAXSTR];
        time_t now = time(0);

        strftime(date, MAXSTR, TIME_FORMAT, localtime(&now));
        return date;
}

static void XSetRoot(const char *name){
        Display *display;

        if (( display = XOpenDisplay(0x0)) == NULL ) {
                fprintf(stderr, "[barM] cannot open display!\n");
                exit(1);
        }

        XStoreName(display, DefaultRootWindow(display), name);
        XSync(display, 0);

        XCloseDisplay(display);
}

int main(void){
	int err, ret;
	char status[MAXSTR];
	struct pollfd *pollfds = NULL;
        int nfds = 0, n;
	unsigned short revents;

	err = init_mixer_handle();
	update_vol();
        get_cpuoccupy((cpu_occupy_t *)&cpu_stat1);

        for(;;){
		n = snd_mixer_poll_descriptors_count(mixer);
		if(n != nfds) {
			free(pollfds);
			nfds = n;
			pollfds = calloc(nfds, sizeof *pollfds);
		}

		err = snd_mixer_poll_descriptors(mixer, &pollfds[0], nfds);
		if (err < 0){
			fprintf(stderr, "cannot get poll descriptors");
			return 1;
		}

		n = poll(pollfds, nfds, LOOP_TIMEOUT);

		if(n <0 ){
			XSetRoot("get VOL error！！！(poll)");
			break;
		}

		if(n > 0){
			err = snd_mixer_poll_descriptors_revents(mixer, &pollfds[0], nfds, &revents);

			if (err < 0 || (revents & (POLLERR | POLLNVAL))){
				XSetRoot("get VOL error！！！");
				break;
			}
			if (revents & POLLIN)
	                	snd_mixer_handle_events(mixer);

		/*	
			if (controls_changed) {
				XSetRoot("get VOL error！！！(control_changed)");
				break;
			}
			*/
			if (control_values_changed) {
				control_values_changed = 0;
				update_vol();
				fprintf(stderr, "%d\n", n);
			}

		}

                int left=sizeof(status),i;
                char *sta = status;
                for(i = 0; i<sizeof(functab)/sizeof(functab[0]); ++i ) {
                        ret=snprintf(sta,left,"%s | ",functab[i]());
                        sta+=ret;
                        left-=ret;
                        if(sta>=(status+MAXSTR))/*When snprintf has to resort to truncating a string it will return the length as if it were not truncated.*/
                                break;
                }
                XSetRoot(status);
        }
        return 0;
}
