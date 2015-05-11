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
	float* frameTime;
	IplImage * image;
	int * wait;
 	pthread_cond_t * can_read; 
	pthread_mutex_t * mutex;
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
	printf("%f\n", *(params->frameTime));
	*(params->frameTime) = 0.f;
	pthread_mutex_unlock(params->mutex);
	*(params->wait) = 0;
	return img;
}

int main(int argc, char** argv )
{
	cvNamedWindow("Webcam", 1);
	
	CvCapture *cam = cvCaptureFromCAM(0);
	//cvSetCaptureProperty(cam, CV_CAP_PROP_AUTO_EXPOSURE, 0);
	cvSetCaptureProperty(cam, CV_CAP_PROP_EXPOSURE,1);
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t can_read = PTHREAD_COND_INITIALIZER;
	IplImage * image = cvQueryFrame(cam);
	int *wait = (int*)malloc(sizeof(int)); *wait = 0;
	float *frameTime = (float*)malloc(sizeof(float)); *frameTime = 0.f;
	
	pthread_t readThread;	
	struct readCameraThreadObj threadParams;
	threadParams.cam = cam;
	threadParams.image = image;
	threadParams.wait = wait;
	threadParams.can_read = &can_read;
	threadParams.mutex = &mutex;
	
	threadParams.image = cvQueryFrame(threadParams.cam);
	pthread_create(&readThread, NULL, &readCamera, (void*)&threadParams);
	
	int n = 0;
	float totalTime = 0.f;
	struct timespec timeobj;
	clock_gettime(CLOCK_MONOTONIC, &timeobj);
	float lastTime = timeobj.tv_nsec/1000000;
	float currTime = lastTime;
	while(cvWaitKey(1)!=27) 
	{	
		struct getImageObj imageParams;
		imageParams.frameTime = frameTime;
		imageParams.image = image;
		imageParams.wait = wait;
		imageParams.can_read = &can_read;
		imageParams.mutex = &mutex;

		cvShowImage("Webcam", getImage(&imageParams));
		clock_gettime(CLOCK_MONOTONIC, &timeobj);
		currTime = timeobj.tv_nsec/1000000;
		
		*frameTime += currTime - lastTime;
		totalTime += *frameTime;
		++n;
		lastTime = currTime;
	}
	printf("AVG FPS: %f\n", totalTime/n);
	cvReleaseCapture(&cam);
	cvDestroyWindow("Webcam");	

	return 0;
}

