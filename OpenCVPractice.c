#include <stdio.h>
#include <pthread.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <libv4l2.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>

//To Do List: 
//Read Mesh and draw into frame
//Track Cricles or track colors?
//Track two points and keep track of object using them

struct readCameraThreadObj
{
	CvCapture *cam;
	IplImage * image;
	int * wait;
 	pthread_cond_t * can_read; 
	pthread_mutex_t * mutex;
};

struct getImageObj
{
	IplImage * image;
	int * wait;
 	pthread_cond_t * can_read; 
	pthread_mutex_t * mutex;
	int* frames;
};

void initializeCamSettings()
{
	int descriptor = open("/dev/video0", O_RDWR);
	
	struct v4l2_control c;
	c.id = V4L2_CID_AUTO_WHITE_BALANCE;
	c.value = 0;
	ioctl(descriptor, VIDIOC_S_CTRL, &c);	

	c.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
	c.value = 0;
	ioctl(descriptor, VIDIOC_S_CTRL, &c);
}

void resetCamSettings()
{
	int descriptor = open("/dev/video0", O_RDWR);
	
	struct v4l2_control c;
	c.id = V4L2_CID_AUTO_WHITE_BALANCE;
	c.value = 1;
	ioctl(descriptor, VIDIOC_S_CTRL, &c);	
	
	c.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
	c.value = 1;
	ioctl(descriptor, VIDIOC_S_CTRL, &c);
}

void *readCamera(void *voidParams)
{
	struct readCameraThreadObj* params = voidParams;
	while(1)
	{	
		if (*(params->wait)==0)
		{
			pthread_mutex_lock(params->mutex);
			params->image = cvQueryFrame(params->cam);
			pthread_mutex_unlock(params->mutex);
		}
		else
			pthread_cond_signal(params->can_read);
	}
	pthread_exit(NULL);
	return NULL;
}

IplImage *getImage(struct getImageObj * params)
{	
	*(params->wait) = 1;
	pthread_mutex_lock(params->mutex);	
	pthread_cond_wait(params->can_read, params->mutex);
	IplImage *img = params->image;
	*(params->wait) = 0;
	++*(params->frames);
	pthread_mutex_unlock(params->mutex);
	return img;
}

IplImage *isolateGreen(IplImage * inputImg)
{
	IplImage *convertedImg = cvCreateImage(cvGetSize(inputImg), 8, 3);
	cvCvtColor(inputImg, convertedImg, CV_BGR2HSV);
	IplImage * img = cvCreateImage(cvGetSize(inputImg), 8, 1);
	CvScalar lower; lower.val[0] = 32.5; lower.val[1] = 60; lower.val[2] = 60; lower.val[3] = 0;
	CvScalar higher; higher.val[0] = 82.5; higher.val[1] = 255; higher.val[2] = 255; higher.val[3] = 0;
	cvInRangeS(convertedImg, lower, higher, img);
	cvReleaseImage(&convertedImg);
	return img;
}

IplImage *isolateBlue(IplImage * inputImg)
{
	IplImage *convertedImg = cvCreateImage(cvGetSize(inputImg), 8, 3);
	cvCvtColor(inputImg, convertedImg, CV_BGR2HSV);
	IplImage * img = cvCreateImage(cvGetSize(inputImg), 8, 1);
	CvScalar lower; lower.val[0] = 90; lower.val[1] = 60; lower.val[2] = 60; lower.val[3] = 0;
	CvScalar higher; higher.val[0] = 130; higher.val[1] = 255; higher.val[2] = 255; higher.val[3] = 0;
	cvInRangeS(convertedImg, lower, higher, img);
	cvReleaseImage(&convertedImg);
	return img;
}

int main(int argc, char** argv )
{
	initializeCamSettings();

	CvCapture *cam = cvCaptureFromCAM(0);
	
	cvNamedWindow("Webcam", 1);
	cvNamedWindow("GreenAndBlueIsolated", 1);
	
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t can_read = PTHREAD_COND_INITIALIZER;
	IplImage * image = cvQueryFrame(cam);
	int wait = 0;
	
	pthread_t readThread;	
	struct readCameraThreadObj threadParams;
	threadParams.cam = cam;
	threadParams.image = image;
	threadParams.wait = &wait;
	threadParams.can_read = &can_read;
	threadParams.mutex = &mutex;
	
	pthread_create(&readThread, NULL, &readCamera, (void*)&threadParams);
	
	int frames = 0;
	float totalTime = 0.f;
	struct timespec timeobj;
	clock_gettime(CLOCK_MONOTONIC, &timeobj);
	float lastTime = timeobj.tv_sec*1000.f + timeobj.tv_nsec/1000000.f;
	float currTime = lastTime;
	char key = 0;
	
	struct getImageObj imageParams;
	imageParams.image = image;
	imageParams.wait = &wait;
	imageParams.can_read = &can_read;
	imageParams.mutex = &mutex;
	imageParams.frames = &frames;
	IplImage * webCamFrame = getImage(&imageParams);
	IplImage * greenFrame = isolateGreen(webCamFrame);
	IplImage * blueFrame = isolateBlue(webCamFrame);
	
	while(key!=27) 
	{	
		imageParams.image = image;
		imageParams.wait = &wait;
		imageParams.can_read = &can_read;
		imageParams.mutex = &mutex;
		imageParams.frames = &frames;
		webCamFrame = getImage(&imageParams);
		greenFrame = isolateGreen(webCamFrame);
		blueFrame = isolateBlue(webCamFrame);
		
		cvShowImage("Webcam", webCamFrame);
		cvAdd(greenFrame, blueFrame, greenFrame, NULL);
		cvShowImage("GreenAndBlueIsolated", greenFrame);
		clock_gettime(CLOCK_MONOTONIC, &timeobj);
		key = cvWaitKey(1);

		if (key==99)
			resetCamSettings();
		if (key==118)
			initializeCamSettings();
		
	}
	currTime = timeobj.tv_sec*1000.f + timeobj.tv_nsec/1000000.f;
	totalTime = currTime - lastTime;
	printf("AVG FPS: %f\n", (1000.f*(float)frames/totalTime));
	cvReleaseCapture(&cam);
	cvDestroyWindow("Webcam");	
	cvDestroyWindow("GreenAndBlueIsolated");
		
	return 0;
}
