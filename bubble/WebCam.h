#pragma once

#include <Windows.h>
#include <atomic>
#include "opencv2/opencv.hpp"

class CWebCam
{
public:
	CWebCam();
	~CWebCam();

	static void captureThread(void*);

	cv::Mat matFrame;				// 映像
	int iFrame = -1;				// フレームNo.
	cv::Size size;					// 映像サイズ
	std::atomic<bool> bOpened{false};	// カメラオープン済みフラグ
	std::atomic<bool> bExec{false};		// 実行中フラグ
	CRITICAL_SECTION csFrame;		// 映像取得用クリティカルセクション
	HANDLE hOpenedEvent;			// カメラオープン完了イベント

};

