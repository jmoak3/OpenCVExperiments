#include <stdio.h>
#include <pthread.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

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
	CvScalar lower; lower.val[0] = 30; lower.val[1] = 80; lower.val[2] = 80; lower.val[3] = 0;
	CvScalar higher; higher.val[0] = 46; higher.val[1] = 255; higher.val[2] = 255; higher.val[3] = 0;
	cvInRangeS(convertedImg, lower, higher, img);
	cvReleaseImage(&convertedImg);
	return img;
}

int main(int argc, char** argv )
{
	cvNamedWindow("Webcam", 1);
	cvNamedWindow("GreenIsolated", 1);
	
	CvCapture *cam = cvCaptureFromCAM(0);
	//cvSetCaptureProperty(cam, CV_CAP_PROP_AUTO_EXPOSURE, 0);
	cvSetCaptureProperty(cam, CV_CAP_PROP_EXPOSURE,1);
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
	
	threadParams.image = cvQueryFrame(threadParams.cam);
	pthread_create(&readThread, NULL, &readCamera, (void*)&threadParams);
	
	int frames = 0;
	float totalTime = 0.f;
	struct timespec timeobj;
	clock_gettime(CLOCK_MONOTONIC, &timeobj);
	float lastTime = timeobj.tv_sec*1000.f + timeobj.tv_nsec/1000000.f;
	float currTime = lastTime;
	char key = 0;
	while(key!=27) 
	{	
		struct getImageObj imageParams;
		imageParams.image = image;
		imageParams.wait = &wait;
		imageParams.can_read = &can_read;
		imageParams.mutex = &mutex;
		imageParams.frames = &frames;
		IplImage * webCamFrame = getImage(&imageParams);
		IplImage * greenFrame = isolateGreen(webCamFrame);
		
		cvShowImage("Webcam", webCamFrame);
		cvShowImage("GreenIsolated", greenFrame);
		clock_gettime(CLOCK_MONOTONIC, &timeobj);
		
		key = cvWaitKey(1);
	}
	currTime = timeobj.tv_sec*1000.f + timeobj.tv_nsec/1000000.f;
	totalTime = currTime - lastTime;
	printf("AVG FPS: %f\n", (1000.f*(float)frames/totalTime));
	cvReleaseCapture(&cam);
	cvDestroyWindow("Webcam");	
	cvDestroyWindow("GreenIsolated");	
	
	return 0;
}

