#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<sys/time.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdarg.h>
#include<dirent.h>
#include"tl_study.h"

static int hc165_fd;
uint8_t g_online_num = 0;
static TrafficLight g_trafficlight[4][3];
static TrafficLightPos g_online[12];
static FILE* log_file;
static char log_name[50];
int write_log (FILE* pFile, const char *format, ...) {
	va_list arg;
	int done;

	va_start (arg, format);
	//done = vfprintf (stdout, format, arg);

	time_t time_log = time(NULL);
	struct tm* tm_log = localtime(&time_log);
	fprintf(pFile, "%04d-%02d-%02d %02d:%02d:%02d ", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday, tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec);

	done = vfprintf (pFile, format, arg);
	va_end (arg);

	fflush(pFile);
	return done;
}

uint32_t HC165D1_Read()
{
	uint32_t ret = 0;
	unsigned char buf[5];
	
	read(hc165_fd, buf, 5);
	//printf("hc165_read: %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
	ret = (ret | (buf[2] & 0x3))<<8;
	ret = (ret | buf[3]) << 8;
	ret = ret | buf[4];
	return ret;
}

uint32_t HC165D2_Read()
{
	uint32_t ret = 0;
	unsigned char buf[5];
	
	read(hc165_fd, buf, 5);
	ret = (ret | buf[0]) << 8;
	ret = (ret | buf[1]) << 8;
	ret = (ret | buf[2]) >> 2;

	return ret;
}

uint8_t trafficLightNum()
{
	uint8_t i, j;
	uint32_t ret = 0, status;
	
	//东西方位
	ret = HC165D1_Read() & 0x00ffffff;
	printf("ret1 %x\n", ret);
	for(i = 0; i < 6; i++)
	{
		for(j = i*3; j < i*3+3; j++)
		{
			status = ret & (0x01 << j);
			if(status == 0)
			{
				g_online_num++;
				g_online[g_online_num - 1].dir = (Direction)(i / 3);
				g_online[g_online_num - 1].to = (Toward)(i % 3);
				break;
			}
		}
	}
	
	//南北方位
	ret = HC165D2_Read() & 0x00ffffff;
	printf("ret2 %x\n", ret);
	for(i = 0; i < 6; i++)
	{
		for(j = i*3; j < i*3+3; j++)
		{
			status = ret & (0x01 << j);
			if(status == 0)
			{
				g_online_num++;
				g_online[g_online_num - 1].dir = (Direction)(i/3 + 2);
				g_online[g_online_num - 1].to = (Toward)(i%3);
				break;
			}
		}
	}
	//g_online_num = 8;
	return g_online_num;
}

void trafficLightRead(uint8_t online_num)
{
	uint8_t i = 0, j, zero_num = 0;
	uint8_t dir, to, led_num;
	uint32_t ret1 = 0, ret2 = 0, status = 0;
	
	//printf("HC165D1 %x\n", ret1);
	
	for(i = 0; i < online_num; i++)
	{
		led_num = g_online[i].dir * 3 + g_online[i].to;
		dir = g_online[i].dir;
		to = g_online[i].to;
		
		if(led_num < 6)
		{
			ret1 = HC165D1_Read() & 0x00ffffff;
			ret2 = led_num * 3;
		}
		else
		{
			ret1 = HC165D2_Read() & 0x00ffffff;
			ret2 = (led_num - 6) * 3;
		}
//		printf("read value %d  led %d dir %d to %d %x\n", i, led_num, dir, to, ret1);
		zero_num = 0;
		
		//判断红绿灯是否有两个以上的灯亮，若果有就舍弃
		for(j = ret2; j < ret2 + 3; j++)
		{
			status = ret1 & (0x01 << j);
			if(status == 0)
			{
				zero_num++;
			}
			
			//红绿灯不可能出现同时两个灯亮
			if(zero_num > 1)
			{
			//	printf("red light status error:%d dir %d to %d %x\n", zero_num, dir, to, ret1);
				g_trafficlight[dir][to].error_num++;
				if(g_trafficlight[dir][to].status == 1)
					g_trafficlight[dir][to].error_num = 0;
				g_trafficlight[dir][to].status = 2;		//同时出现两个灯及以上的灯亮
				if(g_trafficlight[dir][to].error_num == 2)
				{
					printf("g_trafficlight status error, need init:%d dir %d to %d %x\n", g_trafficlight[dir][to].error_num, dir, to, ret1);
					g_trafficlight[dir][to].light_period[red] = 0;
					g_trafficlight[dir][to].light_period[green] = 0;
					g_trafficlight[dir][to].light_period[yellow] = 0;
					g_trafficlight[dir][to].study_flag = 1;
					g_trafficlight[dir][to].last_light = g_trafficlight[dir][to].current_light;
					g_trafficlight[dir][to].start_ticks = 0;
				}
				return;
			}
			else
				g_trafficlight[dir][to].status = 1;			//红绿灯状态正常
		}
		
		for(j = ret2; j < ret2 + 3; j++)
		{
			status = ret1 & (0x01 << j);

			if(status == 0)
			{
				g_trafficlight[dir][to].last_light = g_trafficlight[dir][to].current_light;
				g_trafficlight[dir][to].current_light = (LightType)(j % 3);
			}
		}
	}
}

void trafficLightStudy(uint8_t online_num)
{
	uint8_t i;
	uint8_t dir, to;
	struct timeval tv;
	
	for(i = 0; i < online_num; i++)
	{
		dir = g_online[i].dir;
		to = g_online[i].to;
		if(g_trafficlight[dir][to].study_flag == 1)
		{
			g_trafficlight[dir][to].study_num++;
			//如果学习的时间超过5分钟，则认为学习完成，且红绿灯状态一直为常量
			if(g_trafficlight[dir][to].study_num > 30000)
			{
				g_trafficlight[dir][to].study_flag = 0;
				g_trafficlight[dir][to].status = 3;
				printf("time out, dir %d to %d always on\n", dir, to);
			}				
			
			//若上次红绿灯状态与当前红绿灯状态不一致，则认为状态发生改变
			if(g_trafficlight[dir][to].last_light != g_trafficlight[dir][to].current_light)
			{
				gettimeofday(&tv,NULL);
				g_trafficlight[dir][to].current_ticks_study = tv.tv_sec*100 + tv.tv_usec/10000;
				//测得正常状态下的红绿灯start_ticks不能为0
				if(g_trafficlight[dir][to].start_ticks_study != 0)
				{
					//计算出当前红绿灯周期
					g_trafficlight[dir][to].light_period_study[g_trafficlight[dir][to].last_light] = (g_trafficlight[dir][to].current_ticks_study - g_trafficlight[dir][to].start_ticks_study + 50) / 100;
					g_trafficlight[dir][to].light_period[g_trafficlight[dir][to].last_light] = g_trafficlight[dir][to].light_period_study[g_trafficlight[dir][to].last_light];
					g_trafficlight[dir][to].total_ticks = g_trafficlight[dir][to].current_ticks_study - g_trafficlight[dir][to].start_ticks_study;
					printf("dir %d to %d, light %d total_ticks %lld\n", dir, to, g_trafficlight[dir][to].current_light, g_trafficlight[dir][to].total_ticks);
				}
				gettimeofday(&tv,NULL);
				g_trafficlight[dir][to].start_ticks_study = tv.tv_sec*100 + tv.tv_usec/10000;
				//只有红绿黄周期都测出来，学习结束
				if(g_trafficlight[dir][to].light_period_study[red] && g_trafficlight[dir][to].light_period_study[green] && g_trafficlight[dir][to].light_period_study[yellow])
				{
					printf("dir %d to %d,study is ok\n", dir, to);
					g_trafficlight[dir][to].start_study_flag = 0;
					printf("study period:red %d green %d yellow %d\n", g_trafficlight[dir][to].light_period[red], g_trafficlight[dir][to].light_period[green], g_trafficlight[dir][to].light_period[yellow]);
					write_log(log_file, "dir %d to %d,study is ok,study period:red %d green %d yellow %d\n", dir, to, g_trafficlight[dir][to].light_period[red], g_trafficlight[dir][to].light_period[green], g_trafficlight[dir][to].light_period[yellow]);
					g_trafficlight[dir][to].study_flag = 0;
					g_trafficlight[dir][to].status = 0;
				}
			}
			
		}
	}
}

void trafficLight_study_once(uint8_t online_num)
{
	uint8_t i;
	uint8_t dir, to;
	
	for(i = 0; i < online_num; i++)
	{
		dir = g_online[i].dir;
		to = g_online[i].to;
		g_trafficlight[dir][to].study_num = 0;
		g_trafficlight[dir][to].study_flag = 1;
		g_trafficlight[dir][to].start_ticks_study = 0;
		g_trafficlight[dir][to].total_ticks_study = 0;
		g_trafficlight[dir][to].current_ticks_study = 0;
		g_trafficlight[dir][to].status = 0;
		//g_g_trafficlight[dir][to].start_ticks = 0;
		g_trafficlight[dir][to].light_period_study[red] = 0;
		g_trafficlight[dir][to].light_period_study[green] = 0;
		g_trafficlight[dir][to].light_period_study[yellow] = 0;
	}
}

void trafficLightWork(uint8_t online_num)
{
	uint8_t i, count = 0;
	uint8_t dir, to;
	struct timeval tv;

	for(i = 0; i < online_num; i++)
	{
			dir = g_online[i].dir;
			to = g_online[i].to;
	
		if(g_trafficlight[dir][to].start_study_flag == 0)
		{
			if(g_trafficlight[dir][to].last_light != g_trafficlight[dir][to].current_light)
			{
				gettimeofday(&tv,NULL);
				g_trafficlight[dir][to].start_ticks = tv.tv_sec*100 + tv.tv_usec/10000;
			}
			
			count++;
		}
	}
}

uint8_t checkSum(uint8_t *pbuf, uint16_t len)
{
	uint8_t ret = 0;
	uint16_t i;
	for(i = 0; i < len; i++)
	{
		ret = ret + pbuf[i];
	}
	return ret;
}

int trafficLightStatus(LightInfo *light_info, uint8_t num)
{
	uint8_t dir, to;
	uint32_t total_ticks = 0;
	//uint8_t ret;
	uint8_t sec;
	struct timeval tv;
	dir = g_online[num].dir;
	to = g_online[num].to;
	
	light_info->direct = (Direction)dir;
	light_info->to = (Toward)to;
	light_info->light = g_trafficlight[dir][to].current_light;
	gettimeofday(&tv,NULL);
	total_ticks = tv.tv_sec*100 + tv.tv_usec/10000 - g_trafficlight[dir][to].start_ticks;
	//ret = total_ticks % 100;
	sec = total_ticks / 100;
	
	#if 0
	if(sec == g_trafficlight[dir][to].current_seconds)
	{
		g_trafficlight[dir][to].current_seconds = sec;
		return 2;
	}
	#endif
	g_trafficlight[dir][to].current_light_sec = g_trafficlight[dir][to].current_light;
	g_trafficlight[dir][to].current_seconds = sec;
	light_info->last_seconds = g_trafficlight[dir][to].light_period[light_info->light] - total_ticks / 100 - 1;
	g_trafficlight[dir][to].last_seconds[light_info->light] = light_info->last_seconds; 
	
	if(light_info->last_seconds == 0)
	{
		light_info->light++;
		if(light_info->light > 2)
			light_info->light = (LightType)0;
		light_info->last_seconds = g_trafficlight[dir][to].light_period[light_info->light];
	}
	
	
		/* ºìÂÌµÆ³£ÁÁ£¬ÔòÊ£ÓàÊ±¼äÎª255 */
		if(g_trafficlight[dir][to].status == 3)
			light_info->last_seconds = 255;
		
		if(light_info->light == red)
		{
			light_info->light = 'R';
			light_info->next_light = 'G';
			//printf("trafficlight：dir %d to %d;R %02ds %d sec %d %d %d\n", dir, to, light_info->last_seconds, ret, sec, g_g_trafficlight[dir][to].last_light_sec, g_g_trafficlight[dir][to].current_light_sec);
			printf("trafficlight:dir %d to %d,R %02ds\n", dir, to, light_info->last_seconds);
			write_log(log_file, "trafficlight:dir %d to %d,R %02ds\n", dir, to, light_info->last_seconds);
		}
		else if(light_info->light == green)
		{
			light_info->light = 'G';
			light_info->next_light = 'Y';
			//printf("trafficlight£ºdir %d to %d:G %02ds %d sec %d %d %d\n", dir, to, light_info->last_seconds, ret, sec, g_g_trafficlight[dir][to].last_light_sec, g_g_trafficlight[dir][to].current_light_sec);
			printf("trafficlight:dir %d to %d,G %02ds\n", dir, to, light_info->last_seconds);
			write_log(log_file, "trafficlight:dir %d to %d,G %02ds\n", dir, to, light_info->last_seconds);
		}
		else
		{
			light_info->light = 'Y';
			light_info->next_light = 'R';
			//printf("trafficlight£ºdir %d to %d:Y %02ds %d sec %d %d %d\n", dir, to, light_info->last_seconds, ret, sec, g_g_trafficlight[dir][to].last_light_sec, g_g_trafficlight[dir][to].current_light_sec);
			printf("trafficlight:dir %d to %d,Y %02ds\n", dir, to, light_info->last_seconds);
			write_log(log_file, "trafficlight:dir %d to %d,Y %02ds\n", dir, to, light_info->last_seconds);
		}
		
		light_info->to++;			//协议上方向是123，程序中012
		if(light_info->last_seconds > 200)
			light_info->head1 = 0xA4;			//丢弃该帧
		else
			light_info->head1 = 0xA5;
		
		light_info->head2 = 0x5A;
		light_info->code1 = 0x83;
		light_info->code2 = 0xA0;
		light_info->code3 = 0x00;
		light_info->len = sizeof(LightInfo);
		light_info->tail1 = 0x5A;
		light_info->tail2 = 0xA5;
		#if 1
		//if((g_g_trafficlight[dir][to].study_flag == 1 && g_g_trafficlight[dir][to].start_study_flag == 1) || g_g_trafficlight[dir][to].status == 3)
		if(g_trafficlight[dir][to].start_study_flag == 1 || g_trafficlight[dir][to].status == 3)
		{
			light_info->last_seconds = 255;
		}
		else
		{
			light_info->red_period = g_trafficlight[dir][to].light_period[red];
			light_info->green_period = g_trafficlight[dir][to].light_period[green];
			light_info->yellow_period = g_trafficlight[dir][to].light_period[yellow];
		}
		light_info->check = checkSum((uint8_t *)light_info, sizeof(LightInfo) - 3);

		#endif
		g_trafficlight[dir][to].last_light_sec = g_trafficlight[dir][to].current_light_sec;

		return 1;
}

void send_trafficlight_info(uint8_t *buf, uint8_t online_num)
{
	uint8_t i;
	LightInfo light_info;
	for(i = 0; i < online_num; i++)
	{
		trafficLightStatus(&light_info, i);
		memcpy(buf+sizeof(LightInfo) * i, &light_info, sizeof(LightInfo));
	}
	
}


int readFileList(char *basePath)
{
    DIR *dir;
    struct dirent *ptr;
    char base[50];
    int count = 0;

    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        exit(1);
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 8)    ///file
            printf("d_name:%s/%s\n",basePath,ptr->d_name);
        else if(ptr->d_type == 10)    ///link file
            printf("d_name:%s/%s\n",basePath,ptr->d_name);
        else if(ptr->d_type == 4)    ///dir
        {
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            readFileList(base);
        }
        if(count == 0)
        {
        	memset(base, 0, sizeof(base));
        	sprintf(base, "/home/root/log/%s", ptr->d_name);
        }
        count++;
        if(count > 7)
        {
        	system("/home/root/rm_log.sh");
        	//system("ls -lrt| awk '{print $9}'| head -n 2 | xargs rm -rf");
        	//system("cd -");
        }
        	
       // printf("%s\n", ptr->d_name);
    }
    closedir(dir);
    return count;
}

void update_logfile()
{
	char buf[50];
	time_t time_log = time(NULL);
	int file_num = 0;
	//char basePath[100];
	struct tm* tm_log = localtime(&time_log);
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "/home/root/log/%04d-%02d-%02d", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday);
	if(memcmp(log_name, buf, sizeof(buf)) != 0)
	{
		printf("create a new file\n");
		fclose(log_file);
		log_file = fopen(buf, "a");
		if(log_file == NULL)
			printf("open %s error\n", log_name);
		else
			printf("open %s is ok\n", log_name);
		
		memcpy(log_name, buf, sizeof(buf));
	}

	//readFileList("/home/root/log/");
}

 

void *trafficlight_pthread(void *arg)
{
	uint16_t count = 0;
	while(1)
	{
		count++;
		if(count > 50000)
		{
			count = 0;
			trafficLight_study_once(g_online_num);
			update_logfile();
		}
		
		update_logfile();
		trafficLightRead(g_online_num);
		trafficLightStudy(g_online_num);
		trafficLightWork(g_online_num);
		usleep(10*1000);
	}
}

pthread_t trafficLightInit()
{
	uint8_t i, j;
	uint8_t dir, to;
	uint8_t res1 = 1, res2 = 0;
	pthread_t tid;

	time_t time_log = time(NULL);
	struct tm* tm_log = localtime(&time_log);
	
	memset(log_name, 0, sizeof(log_name));
	sprintf(log_name, "/home/root/log/%04d-%02d-%02d", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday);
	log_file = fopen(log_name, "a");
	if(log_file == NULL)
		printf("open %s error\n", log_name);
	else
		printf("open %s is ok\n", log_name);
	
		
	hc165_fd = open("/dev/hc165", O_RDWR, S_IRUSR|S_IWUSR);
	if(hc165_fd == -1)
	{
		printf("can't open hc165\n");
		return -1;
	}
	memset(g_trafficlight, 0, 12 * sizeof(TrafficLight));
	
	//初始化将红绿灯状态置为unknown
	for(i = 0; i < 4; i++)
		for(j = 0; j < 3; j++)
			g_trafficlight[i][j].current_light = unknown;
	
	//trafficLightNum();
	#if 1
	while(res1 != res2)
	{
		res1 = trafficLightNum();		//测出红绿灯在线数及方位
		g_online_num = 0;
		usleep(200*1000);
		res2 = trafficLightNum();
	}
	#endif
	printf("Tips:\n");
	printf("east wset south north:0123\n");
	printf("left font right:012\n");
	printf("trafficlight online: %d\n", g_online_num);
	trafficLightRead(g_online_num);	
	for(i = 0; i < g_online_num; i++)
	{
		dir = g_online[i].dir;
		to = g_online[i].to;
		g_trafficlight[dir][to].study_flag = 1;
		g_trafficlight[dir][to].start_study_flag = 1;
		g_trafficlight[dir][to].last_light = g_trafficlight[dir][to].current_light;
		g_trafficlight[dir][to].current_seconds = 255;
		printf("dir %d to %d light %d %d\n", dir, to, g_trafficlight[dir][to].current_light, g_trafficlight[dir][to].study_flag);
	}	
	
	pthread_create(&tid,NULL,trafficlight_pthread, NULL);
	
	printf("trafficlight init success\n");
	return tid;
}