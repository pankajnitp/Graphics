#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include <TCHAR.h>
#include <math.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "imageloader.h"
/******************************************************************************
Defines
******************************************************************************/
// Windows class name to register
#define	WINDOW_CLASS _T("PVRShellClass")

// Width and height of the window
#define WINDOW_WIDTH	960
#define WINDOW_HEIGHT	540

// Index to bind the attributes to vertex shaders
#define VERTEX_ARRAY	0
#define PI 3.14159
#define RADIUS 0.3

#define VERTEX_LIMIT	1500
#define START_TOP		0.0f
#define END_TOP			180.0f
#define START_RIGHT		-90.0f
#define END_RIGHT		90.0f
#define INC_ANGLE		1
#define SCALE_RESET			1.0f
#define COUNT_RESET			0
#define SCALE_AFTER_FRAME	20
#define SCALE_FACTOR		1.005f
#define SCALE_LIMIT			1.3f
/******************************************************************************
Global variables
******************************************************************************/

// Variable set in the message handler to finish the demo
bool	g_bDemoDone = false;
float angle = -45.0f;

/*!****************************************************************************
@Function		WndProc
@Input			hWnd		Handle to the window
@Input			message		Specifies the message
@Input			wParam		Additional message information
@Input			lParam		Additional message information
@Return		LRESULT		result code to OS
@Description	Processes messages for the main window
******************************************************************************/
#ifndef NO_GDI
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		// Handles the close message when a user clicks the quit icon of the window
	case WM_CLOSE:
		g_bDemoDone = true;
		PostQuitMessage(0);
		return 1;
	case WM_KEYDOWN:
	{
		break;
	}

	default:
		break;
	}

	// Calls the default window procedure for messages we did not handle
	return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif
/*!****************************************************************************
@Function		TestEGLError
@Return		bool			true if no EGL error was detected
@Description	Tests for an EGL error and prints it
******************************************************************************/
bool TestEGLError()
{
	EGLint iErr = eglGetError();
	if (iErr != EGL_SUCCESS)
	{
		return false;
	}
	return true;
}

GLuint LoadShader(char *shaderSrc, GLenum type)
{
	GLuint shader;
	GLint compiled;

	// Create the shader object
	shader = glCreateShader(type);
	if (shader == 0)
		return 0;
	// Load the shader source
	glShaderSource(shader, 1, (const char**)&shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);
	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1)
		{
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			printf("Error compiling shader:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

/*!****************************************************************************
@Function		WinMain
@Input			hInstance		Application instance from OS
@Input			hPrevInstance	Always NULL
@Input			lpCmdLine		command line from OS
@Input			nCmdShow		Specifies how the window is to be shown
@Return		int				result code to OS
@Description	Main function of the program
******************************************************************************/
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, TCHAR *lpCmdLine, int nCmdShow)
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	// Windows variables
	HWND				hWnd = 0;
	HDC					hDC = 0;

	// EGL variables
	EGLDisplay			eglDisplay = 0;
	EGLConfig			eglConfig = 0;
	EGLSurface			eglSurface = 0;
	EGLContext			eglContext = 0;
	EGLNativeWindowType	eglWindow = 0;

	// Matrix used for projection model view (PMVMatrix)

	// Fragment and vertex shaders code
	char* pszFragShader = "\
		void main (void)\
		{\
		    gl_FragColor = vec4(1.0, 0.0, 0.0 ,0.0);\
		}";
	char* pszVertShader = "\
		attribute highp vec4	myVertex;\
		uniform mediump mat4	myPMVMatrix;\
		void main(void)\
		{\
			gl_Position = myPMVMatrix * myVertex ;\
		}";

#ifndef NO_GDI
	// Register the windows class
	WNDCLASS sWC;
	sWC.style = CS_HREDRAW | CS_VREDRAW;
	sWC.lpfnWndProc = WndProc;
	sWC.cbClsExtra = 0;
	sWC.cbWndExtra = 0;
	sWC.hInstance = hInstance;
	sWC.hIcon = 0;
	sWC.hCursor = 0;
	sWC.lpszMenuName = 0;
	sWC.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	sWC.lpszClassName = WINDOW_CLASS;
	unsigned int nWidth = WINDOW_WIDTH;
	unsigned int nHeight = WINDOW_HEIGHT;

	ATOM registerClass = RegisterClass(&sWC);
	if (!registerClass)
	{
		MessageBox(0, _T("Failed to register the window class"), _T("Error"), MB_OK | MB_ICONEXCLAMATION);
	}

	// Create the eglWindow
	RECT	sRect;
	SetRect(&sRect, 0, 0, nWidth, nHeight);
	AdjustWindowRectEx(&sRect, WS_CAPTION | WS_SYSMENU, false, 0);
	hWnd = CreateWindow(WINDOW_CLASS, _T("HelloTriangle"), WS_VISIBLE | WS_SYSMENU,
		0, 0, nHeight, nHeight, NULL, NULL, hInstance, NULL);
	eglWindow = hWnd;

	// Get the associated device context
	hDC = GetDC(hWnd);
	if (!hDC)
	{
		MessageBox(0, _T("Failed to create the device context"), _T("Error"), MB_OK | MB_ICONEXCLAMATION);
		goto cleanup;
	}
#endif

	eglDisplay = eglGetDisplay(hDC);

	if (eglDisplay == EGL_NO_DISPLAY)
		eglDisplay = eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY);

	EGLint iMajorVersion, iMinorVersion;
	if (!eglInitialize(eglDisplay, &iMajorVersion, &iMinorVersion))
	{
#ifndef NO_GDI
		MessageBox(0, _T("eglInitialize() failed."), _T("Error"), MB_OK | MB_ICONEXCLAMATION);
#endif
		goto cleanup;
	}

	eglBindAPI(EGL_OPENGL_ES_API);
	if (!TestEGLError())
	{
		goto cleanup;
	}

	const EGLint pi32ConfigAttribs[] =
	{
		EGL_LEVEL,				0,
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES2_BIT,
		EGL_NATIVE_RENDERABLE,	EGL_FALSE,
		EGL_DEPTH_SIZE,			EGL_DONT_CARE,
		EGL_NONE
	};

	int iConfigs;
	if (!eglChooseConfig(eglDisplay, pi32ConfigAttribs, &eglConfig, 1, &iConfigs) || (iConfigs != 1))
	{
#ifndef NO_GDI
		MessageBox(0, _T("eglChooseConfig() failed."), _T("Error"), MB_OK | MB_ICONEXCLAMATION);
#endif
		goto cleanup;
	}

	eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, eglWindow, NULL);

	if (eglSurface == EGL_NO_SURFACE)
	{
		eglGetError();
		eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, NULL, NULL);
	}

	if (!TestEGLError())
	{
		goto cleanup;
	}

	EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	eglContext = eglCreateContext(eglDisplay, eglConfig, NULL, ai32ContextAttribs);
	if (!TestEGLError())
	{
		goto cleanup;
	}

	eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
	if (!TestEGLError())
	{
		goto cleanup;
	}

	GLuint uiProgramObject;					/* Used to hold the program handle (made out of the two previous shaders */

											// Load the vertex/fragment shaders
	GLuint vertexShader = LoadShader(pszVertShader, GL_VERTEX_SHADER);
	GLuint fragmentShader = LoadShader(pszFragShader, GL_FRAGMENT_SHADER);

	// Create the shader program
	uiProgramObject = glCreateProgram();

	// Attach the fragment and vertex shaders to it
	glAttachShader(uiProgramObject, fragmentShader);
	glAttachShader(uiProgramObject, vertexShader);

	// Bind the custom vertex attribute "myVertex" to location VERTEX_ARRAY
	glBindAttribLocation(uiProgramObject, VERTEX_ARRAY, "myVertex");
	// Link the program
	glLinkProgram(uiProgramObject);

	// Check if linking succeeded in the same way we checked for compilation success
	GLint bLinked;
	glGetProgramiv(uiProgramObject, GL_LINK_STATUS, &bLinked);
	if (!bLinked)
	{
#ifndef NO_GDI
		int i32InfoLogLength, i32CharsWritten;
		glGetProgramiv(uiProgramObject, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		char* pszInfoLog = new char[i32InfoLogLength];
		glGetProgramInfoLog(uiProgramObject, i32InfoLogLength, &i32CharsWritten, pszInfoLog);

		//MessageBox(hWnd, i32InfoLogLength ? pszInfoLog : _T(""), _T("Failed to link program"), MB_OK | MB_ICONEXCLAMATION);
		delete[] pszInfoLog;
#endif
		goto cleanup;
	}

	// Actually use the created program
	glUseProgram(uiProgramObject);
	// Sets the sampler2D variable to the first texture unit
	glUniform1i(glGetUniformLocation(uiProgramObject, "sampler2d"), 0);
	// Sets the clear color.
	// The colours are passed per channel (red,green,blue,alpha) as float values from 0.0 to 1.0
	glClearColor(0.6f, 0.8f, 1.0f, 1.0f);

	// Interleaved vertex data
	//GLfloat afVertices[100];

	//Set a viewport
	glViewport(0, 0, WINDOW_HEIGHT, WINDOW_HEIGHT);
	// Draws a triangle for 800 frames
	// First gets the location of that variable in the shader using its name
	int i32Location = glGetUniformLocation(uiProgramObject, "myPMVMatrix");

	GLfloat afVertices[VERTEX_LIMIT] = { -RADIUS,  RADIUS, 0.0f,  // Position 0
		-RADIUS, -RADIUS, 0.0f,		// Position 1
		RADIUS, -RADIUS, 0.0f,		// Position 2
		RADIUS,  RADIUS, 0.0f,		// Position 3
	};

	GLint countVert = 12;

	for (float i = START_TOP; i <= END_TOP; i += INC_ANGLE)
	{
		afVertices[countVert++] = RADIUS*cos(i*PI / 180.0f);
		afVertices[countVert++] = RADIUS + RADIUS*sin(i*PI / 180.0f);
		afVertices[countVert++] = 0;
	}

	GLfloat cfVertices[500];
	for (float i = START_RIGHT; i <= END_RIGHT; i += INC_ANGLE)
	{
		afVertices[countVert++] = RADIUS + RADIUS*cos(i*PI / 180.0f);
		afVertices[countVert++] = RADIUS*sin(i*PI / 180.0f);
		afVertices[countVert++] = 0;
	}

	float scale = SCALE_RESET;
	int count = COUNT_RESET;
	for (; ;)
	{
		if (g_bDemoDone == true)
			break;

		count++;
		if (count == SCALE_AFTER_FRAME)
		{
			scale = scale * SCALE_FACTOR;
			if (scale >= SCALE_LIMIT)
			{
				scale = SCALE_RESET;
			}
			count = COUNT_RESET;
		}


		float pfzIdentity[] =
		{
			scale * cos(angle*PI / 180.0f), -scale * sin(angle*PI / 180.0f),0.0f,0.0f,
			scale * sin(angle*PI / 180.0f), scale * cos(angle*PI / 180.0f),0.0f,0.0f,
			0.0f,0.0f,scale,0.0f,
			0.0f,0.0f,0.0f,1.0f
		};

		glClear(GL_COLOR_BUFFER_BIT);
		if (!TestEGLError())
		{
			goto cleanup;
		}
		glUniformMatrix4fv(i32Location, 1, GL_FALSE, pfzIdentity);

		// Pass the vertex data
		glEnableVertexAttribArray(VERTEX_ARRAY);

		// Load the vertex position
		glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, afVertices);

		glDrawArrays(GL_TRIANGLE_FAN, 0, countVert / 3);

		EGLint iErr = eglGetError();
		if (iErr != EGL_SUCCESS)
		{
			goto cleanup;
		}

		/*
		Swap Buffers.
		Brings to the native display the current render surface.
		*/
		eglSwapBuffers(eglDisplay, eglSurface);
		iErr = eglGetError();
		if (iErr != EGL_SUCCESS)
		{
			goto cleanup;
		}
#ifndef NO_GDI
		// Managing the window messages
		MSG msg;
		PeekMessage(&msg, hWnd, NULL, NULL, PM_REMOVE);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		//}
#endif
	}

	// Frees the OpenGL handles for the program and the 2 shaders
	glDeleteProgram(uiProgramObject);
	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);

cleanup:
	eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglTerminate(eglDisplay);

#ifndef NO_GDI
	// Release the device context
	if (hDC) ReleaseDC(hWnd, hDC);
	// Destroy the eglWindow
	if (hWnd) DestroyWindow(hWnd);
#endif
	return 0;
}
