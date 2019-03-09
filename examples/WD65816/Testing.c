/*
 * Simple Driver - to drive main schematic
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <GLFW/glfw3.h>
#include <glext.h>
#include "gui/debugger.h"

#define USE_TIMING 1

void TEST(uint8_t clk, uint8_t reset);

#define WIDTH	(48+256+48+96)
#define	HEIGHT	(56+192+56+8)
#define TIMING_WIDTH	640*2
#define TIMING_HEIGHT	900

#define MAX_WINDOWS		(8)

#define MAIN_WINDOW		0
#define TIMING_WINDOW	1

GLFWwindow* windows[MAX_WINDOWS];
unsigned char *videoMemory[MAX_WINDOWS];
GLint videoTexture[MAX_WINDOWS];

void ShowScreen(int windowNum,int w,int h)
{
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);
	
	// glTexSubImage2D is faster when not using a texture range
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,1.0f);

	glTexCoord2f(0.0f, 0.0f+h);
	glVertex2f(-1.0f,-1.0f);

	glTexCoord2f(0.0f+w, 0.0f+h);
	glVertex2f(1.0f,-1.0f);

	glTexCoord2f(0.0f+w, 0.0f);
	glVertex2f(1.0f,1.0f);
	
	glEnd();
	
	glFlush();
}

void setupGL(int windowNum,int w, int h) 
{
	videoTexture[windowNum] = windowNum+1;
	videoMemory[windowNum] = (unsigned char*)malloc(w*h*sizeof(unsigned int));
	memset(videoMemory[windowNum],0,w*h*sizeof(unsigned int));

	//Tell OpenGL how to convert from coordinates to pixel values
	glViewport(0, 0, w, h);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, videoTexture[windowNum]);

	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, w,
			h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, videoMemory[windowNum]);

	glDisable(GL_DEPTH_TEST);
}


void sizeHandler(GLFWwindow* window, int xs, int ys)
{
	glfwMakeContextCurrent(window);
	glViewport(0, 0, xs, ys);
}

#define MAX_KEY_CODE	(512)
unsigned char keyArray[MAX_KEY_CODE * 3];

int KeyDown(int key)
{
	return keyArray[key * 3 + 1] == GLFW_PRESS;
}

int CheckKey(int key)
{
	return keyArray[key * 3 + 2];
}

void ClearKey(int key)
{
	keyArray[key * 3 + 2] = 0;
}

void kbHandler(GLFWwindow* window, int key, int scan, int action, int mod)		/* At present ignores which window, will add per window keys later */
{
	if (key<0 || key >= MAX_KEY_CODE)
		return;

	keyArray[key * 3 + 0] = keyArray[key * 3 + 1];
	if (action == GLFW_RELEASE)
	{
		keyArray[key * 3 + 1] = GLFW_RELEASE;
	}
	else
	{
		keyArray[key * 3 + 1] = GLFW_PRESS;
	}
	keyArray[key * 3 + 2] |= (keyArray[key * 3 + 0] == GLFW_RELEASE) && (keyArray[key * 3 + 1] == GLFW_PRESS);
}

static void glfwError(int id, const char* description)
{
	printf("GLFW ERROR : [%d] %s",id,description);
}

int pause=0;

int main(int argc,char**argv)
{
	uint16_t lastIns=0xAAAA;

	/// Initialize GLFW 
	glfwSetErrorCallback(&glfwError);
	glfwInit(); 

	// Open timing OpenGL window 
	if (!(windows[TIMING_WINDOW] = glfwCreateWindow(TIMING_WIDTH, TIMING_HEIGHT, "Logic Analyser", NULL, NULL)))
	{
		glfwTerminate();
		return 1;
	}

	glfwSetWindowPos(windows[TIMING_WINDOW], 0, 640);

	glfwMakeContextCurrent(windows[TIMING_WINDOW]);
	setupGL(TIMING_WINDOW, TIMING_WIDTH, TIMING_HEIGHT);
	glfwSetWindowSizeCallback(windows[TIMING_WINDOW],sizeHandler);

	glfwSetKeyCallback(windows[TIMING_WINDOW], kbHandler);

	int runFor = 128;
	int resetCnt = 2;
	while (1==1)
	{
		if (runFor == 0)
		{
#if USE_TIMING
			glfwMakeContextCurrent(windows[TIMING_WINDOW]);
			DrawTiming(videoMemory[TIMING_WINDOW], TIMING_WIDTH, TIMING_HEIGHT);
			UpdateTimingWindow(windows[TIMING_WINDOW]);
			ShowScreen(TIMING_WINDOW, TIMING_WIDTH, TIMING_HEIGHT);
			glfwSwapBuffers(windows[TIMING_WINDOW]);
#endif
			glfwPollEvents();
		}
		else
		{
			runFor--;
			TEST(1, resetCnt==0?1:0);
			TEST(0, resetCnt==0?1:0);
			if (resetCnt!=0)
				resetCnt--;
		}
	}

	return 0;
}

