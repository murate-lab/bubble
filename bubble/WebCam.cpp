#include "stdafx.h"
#include "WebCam.h"

#include <Windows.h>

CWebCam::CWebCam()
{
	size = cv::Size(1280, 720);
}

CWebCam::~CWebCam()
{
}

void CWebCam::captureThread(void* inst)
{
	static cv::VideoCapture cap;
	CWebCam* pWebCam;
	cv::Mat matWork;

	pWebCam = (CWebCam*)inst;
	InitializeCriticalSection(&pWebCam->csFrame);

	// カメラオープン
	cap.open(0);
	if (cap.isOpened()) {
		cap.set(cv::CAP_PROP_FPS, 30);
		cap.set(cv::CAP_PROP_FRAME_WIDTH, pWebCam->size.width);
		cap.set(cv::CAP_PROP_FRAME_HEIGHT, pWebCam->size.height);
		pWebCam->bOpened = true;
		pWebCam->bExec = true;
	} else {
		return;
	}

	// 映像取得
	while (pWebCam->bExec) {
		// カメラ映像入力
		cap >> matWork;

		// バッファにコピー
		EnterCriticalSection(&pWebCam->csFrame);
		matWork.copyTo(pWebCam->matFrame);
		pWebCam->iFrame++;
		LeaveCriticalSection(&pWebCam->csFrame);
		//printf("frame : %d\n", pWebCam->iFrame);

		Sleep(0);
	}
}