// bubble.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <process.h>
#include <mmsystem.h>
#include <math.h>
#include <time.h>
#include "opencv2/opencv.hpp"
#include "WebCam.h"

#pragma comment(lib, "winmm.lib")

// 定数定義
#define DIFF_3FRAME					// 3フレーム差分
#define FLUC_WIDTH		100			// ゆらぎ幅

// 構造体定義
struct stBubble {
	int iSizeIdx;					// シャボン玉サイズインデックス
	int iSize;						// シャボン玉サイズ
	int iCenter;					// 横中心
	int iFluc;						// ゆらぎオフセット
	cv::Point poPos;				// 位置
};

// 内部関数定義
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);
void alphaBlend(cv::Mat src, cv::Mat objRGB, cv::Mat objA, cv::Mat& dst, cv::Point po);
void measureTime(int i, char* pcMessage = NULL);

// 内部変数定義
static CWebCam cam;					// カメラクラス
static cv::VideoWriter vw;			// 映像出力

// メイン
int _tmain(int argc, _TCHAR* argv[])
{
	int idx;											// ループインデックス
	cv::Mat matWork;									// 作業用
	cv::Mat matDisp;									// 表示映像
	cv::Mat matBuf[3];									// 映像バッファ
	int iNow = 0;										// 映像バッファの現在のインデックス
	int iLastFrame = -1;								// カメラ映像の最新フレームNo.
	cv::Mat matBub;										// シャボン玉
	std::vector<cv::Mat> vecBub4ch;						// シャボン玉(4ch)
	std::vector<cv::Mat> vecBub3ch;						// シャボン玉(3ch)
	cv::Mat matBubOrgRGB;								// シャボン玉(RGB)（読み込みサイズ）
	cv::Mat matBubOrgA;									// シャボン玉(A)（読み込みサイズ）
	cv::Mat matBubRGB[3];								// シャボン玉(RGB)（３サイズ分）
	cv::Mat matBubA[3];									// シャボン玉(A)（３サイズ分）
	std::vector<struct stBubble> vecBubble;				// シャボン玉リスト	
	std::vector<struct stBubble>::iterator itBubble;	// シャボン玉リストのイテレータ
	struct stBubble bub;								// 生成するシャボン玉
	cv::Mat matGray[3];									// 映像差分作成用グレースケール画像
	cv::Mat matDiffWork[2];								// 差分映像作業用
	cv::Mat matDiff;									// 差分映像
	cv::Mat matIntg;									// 差分映像のインテグラルイメージ
	int iKey;											// 押下キーコード
	int iLeft, iTop, iRight, iBottom;					// シャボン玉当たり判定用座標
	int iSum;											// シャボン玉当たり判定用差分合計値
	MSG msg;											// イベントメッセージ
	int iTimerID;										// タイマID
	int iBubbleSizeMax = 400;							// シャボン玉最大サイズ
	int iBubblePrm;										// シャボン玉壊れやすさパラメータ
	int iScore;											// スコア
	char strScore[64];									// スコア表示
	int iWaitTime = 0;									// 効果音再生ウェイト時間
	int iWaitLevel = 0;									// 効果音再生ウェイトレベル
	time_t timNow;										// 現在日時
	struct tm tmNow;									// 現在日時
	char pcOutput[1024];								// 出力ファイル名
	int iProcFrame = 0;									// 秒間処理フレーム数

	// パラメータ指定
	if (argc > 1) {
		cam.size.width = _ttoi(argv[1]);
	}
	if (argc > 2) {
		cam.size.height = _ttoi(argv[2]);
	}
	if (argc > 3) {
		iBubbleSizeMax = _ttoi(argv[3]);
	}

	// カメラスレッド開始
	printf("カメラオープン中...\n");
	_beginthread(cam.captureThread, 0, (void*)&cam);
	WaitForSingleObject(cam.hOpenedEvent, INFINITE);

	// ハンドラ関数を登録
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);

	// 映像バッファ初期化
	for (idx = 0; idx < 3; idx++) {
		matBuf[idx] = cv::Mat(cam.size, CV_8UC3);
	}
	iNow = 0;

	// シャボン玉読み込み
	matBub = cv::imread("bubble.png", cv::IMREAD_UNCHANGED);
	cv::split(matBub, vecBub4ch);
	vecBub3ch.push_back(vecBub4ch[0]);
	vecBub3ch.push_back(vecBub4ch[1]);
	vecBub3ch.push_back(vecBub4ch[2]);
	cv::merge(vecBub3ch, matBubOrgRGB);
	vecBub4ch[3].copyTo(matBubOrgA);
	for (idx = 0; idx < 3; idx++) {
		cv::resize(matBubOrgRGB, matBubRGB[idx], cv::Size((int)(iBubbleSizeMax / pow(2, idx)), (int)(iBubbleSizeMax / pow(2, idx))));
		cv::resize(matBubOrgA, matBubA[idx], cv::Size((int)(iBubbleSizeMax / pow(2, idx)), (int)(iBubbleSizeMax / pow(2, idx))));
	}

	// 映像出力初期化
	timNow = time(NULL);
	localtime_s(&tmNow, &timNow);
	sprintf_s(pcOutput, 1024, "output%04d%02d%02d%02d%02d%02d.mp4", 1900 + tmNow.tm_year, tmNow.tm_mon + 1, tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
	vw.open(pcOutput, cv::VideoWriter::fourcc('M', 'P', 'G', '1'), 30, cam.size);

	// メインループ
	iTimerID = (int)SetTimer(NULL, 1, 33, NULL);
	iScore = 0;
	srand((unsigned int)time(NULL));
	while (true) {
		// イベント発生判定
		GetMessage(&msg, NULL, 0, 0);
        if ((msg.message == WM_TIMER) && (msg.wParam == iTimerID)) {
			// イベント発生
		} else {
			cv::waitKey(1);
			continue;
		}

		measureTime(0);

		// カメラ映像取得＆鏡面化
		if (cam.matFrame.dims == 0) continue;
		EnterCriticalSection(&cam.csFrame);
		cv::flip(cam.matFrame, matWork, 1);

		// 映像バッファリング
		if (iLastFrame < cam.iFrame) {
			iNow = (iNow + 1) % 3;
			matWork.copyTo(matBuf[iNow]);
			iLastFrame = cam.iFrame;
		}
		LeaveCriticalSection(&cam.csFrame);

		// フレーム差分作成
#ifdef DIFF_3FRAME
		cv::cvtColor(matBuf[(iNow + 1) % 3], matGray[0], cv::COLOR_BGR2GRAY);
		cv::cvtColor(matBuf[(iNow + 2) % 3], matGray[1], cv::COLOR_BGR2GRAY);
		cv::cvtColor(matBuf[iNow          ], matGray[2], cv::COLOR_BGR2GRAY);
		cv::absdiff(matGray[0], matGray[1], matDiffWork[0]);
		cv::absdiff(matGray[1], matGray[2], matDiffWork[1]);
		cv::min(matDiffWork[0], matDiffWork[1], matDiff);
		iBubblePrm = 16;
#else
		cv::cvtColor(matBuf[(iNow + 2) % 3], matGray[0], CV_RGB2GRAY);
		cv::cvtColor(matBuf[iNow          ], matGray[1], CV_RGB2GRAY);
		cv::absdiff(matGray[0], matGray[1], matDiff);
		iBubblePrm = 4;
#endif
		//cv::imshow("diff_dbg", matDiff);

		// インテグラルイメージ作成
		cv::integral(matDiff, matIntg);

		// シャボン玉生成
		if (rand() % 10 == 0) {
			bub.iSizeIdx = (int)(sqrt(rand() % 36) / 2);
			bub.iSize = (int)(iBubbleSizeMax / pow(2, bub.iSizeIdx));
			bub.iCenter = rand() % (matBuf[iNow].cols - bub.iSize);
			bub.iFluc = rand() % 360;
			bub.poPos = cv::Point(bub.iCenter + (int)((double)FLUC_WIDTH *
							sin((double)(bub.iFluc + matBuf[iNow].rows / 2) * CV_PI / 180.0)), matBuf[iNow].rows);
			vecBubble.push_back(bub);
		}

		// シャボン玉移動
		for (idx = 0; idx < (int)vecBubble.size(); idx++) {
			vecBubble[idx].poPos.y -= 8;
			vecBubble[idx].poPos.x = vecBubble[idx].iCenter + (int)((double)FLUC_WIDTH *
										sin((double)(vecBubble[idx].iFluc + vecBubble[idx].poPos.y / 2) * CV_PI / 180.0));
		}

		// シャボン玉消滅判定
		itBubble = vecBubble.begin();
		while (itBubble != vecBubble.end()) {
			if (itBubble->poPos.y + itBubble->iSize < 0) {
				itBubble = vecBubble.erase(itBubble);
			} else {
				itBubble++;
			}
		}

		// シャボン玉当たり判定
		itBubble = vecBubble.begin();
		iWaitTime -= 33;
		while (itBubble != vecBubble.end()) {
			// シャボン玉の見える部分が半分以下なら判定しない
			if (itBubble->poPos.y > cam.size.height - itBubble->iSize / 2) {
				itBubble++;
				continue;
			}

			// シャボン玉領域の差分を算出
			iLeft   = itBubble->poPos.x;
			iTop    = itBubble->poPos.y;
			iRight  = itBubble->poPos.x + itBubble->iSize;
			iBottom = itBubble->poPos.y + itBubble->iSize;
			if (iLeft < 0)                  iLeft = 0;
			if (iTop < 0)                   iTop = 0;
			if (iRight >= cam.size.width)   iRight = cam.size.width - 1;
			if (iBottom >= cam.size.height) iBottom = cam.size.height - 1;
			iSum = matIntg.at<int>(iBottom, iRight) -
				   matIntg.at<int>(iBottom, iLeft) -
				   matIntg.at<int>(iTop, iRight) +
				   matIntg.at<int>(iTop, iLeft);

			// 差分から当たり判定して効果音再生＋シャボン玉消去
			if (iWaitTime < 0) {
				iWaitTime = 0;
				iWaitLevel = 0;
			}
			if (iSum > 128 * (iRight - iLeft) * (iBottom - iTop) / iBubblePrm) {
				switch (itBubble->iSizeIdx) {
				case 0:
					if (iWaitLevel < 3) {
						PlaySound(_T("bubble3.wav"), NULL, SND_FILENAME | SND_ASYNC);
						iWaitTime = 960;
						iWaitLevel = 3;
					}
					break;
				case 1:
					if (iWaitLevel < 2) {
						PlaySound(_T("bubble2.wav"), NULL, SND_FILENAME | SND_ASYNC);
						iWaitTime = 680;
						iWaitLevel = 2;
					}
					break;
				case 2:
					if (iWaitLevel < 1) {
						PlaySound(_T("bubble1.wav"), NULL, SND_FILENAME | SND_ASYNC);
						iWaitTime = 450;
						iWaitLevel = 1;
					}
					break;
				default:
					break;
				}
				itBubble = vecBubble.erase(itBubble);
				iScore++;
			} else {
				itBubble++;
			}
		}

		// シャボン玉表示
		matBuf[iNow].copyTo(matDisp);
		for (idx = 0; idx < (int)vecBubble.size(); idx++) {
			alphaBlend(matDisp, matBubRGB[vecBubble[idx].iSizeIdx], matBubA[vecBubble[idx].iSizeIdx], matDisp, vecBubble[idx].poPos);
		}

		// スコア表示
		sprintf_s(strScore, "Score : %d", iScore);
		cv::putText(matDisp, strScore, cv::Point(0, matDisp.rows - 5), cv::FONT_HERSHEY_SIMPLEX, 1.0, CV_RGB(0, 0, 0), 2, cv::LINE_AA);
		cv::putText(matDisp, strScore, cv::Point(0, matDisp.rows - 3), cv::FONT_HERSHEY_SIMPLEX, 1.0, CV_RGB(0, 0, 0), 2, cv::LINE_AA);
		cv::putText(matDisp, strScore, cv::Point(2, matDisp.rows - 5), cv::FONT_HERSHEY_SIMPLEX, 1.0, CV_RGB(0, 0, 0), 2, cv::LINE_AA);
		cv::putText(matDisp, strScore, cv::Point(2, matDisp.rows - 3), cv::FONT_HERSHEY_SIMPLEX, 1.0, CV_RGB(0, 0, 0), 2, cv::LINE_AA);
		cv::putText(matDisp, strScore, cv::Point(1, matDisp.rows - 4), cv::FONT_HERSHEY_SIMPLEX, 1.0, CV_RGB(0, 255, 255), 2, cv::LINE_AA);

		// 処理フレームレートチェック
		static int iBeforeSec = -1;
		static int iNowSec = 0;
		iProcFrame++;
		timNow = time(NULL);
		localtime_s(&tmNow, &timNow);
		iNowSec = tmNow.tm_sec;
		if (iNowSec != iBeforeSec) {
			//printf("Frame Rate : %d[fps]\n", iProcFrame);
			iProcFrame = 0;
			iBeforeSec = iNowSec;
		}

		// 表示
		cv::imshow("bubble", matDisp);
		vw.write(matDisp);
		iKey = cv::waitKey(1);
		if (iKey == 27) break;	// [Esc]キー

		measureTime(1, "1Frame Time");
	}

	// 後始末
	cam.bExec = false;
	vw.release();
	cv::destroyAllWindows();

	return 0;
}

// アプリ強制終了ハンドラ
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	cam.bExec = false;
	vw.release();
	cv::destroyAllWindows();

	return TRUE;
}

// アルファブレンド
void alphaBlend(cv::Mat src, cv::Mat objRGB, cv::Mat objA, cv::Mat& dst, cv::Point po)
{
	src.copyTo(dst);

	// src/objの重なり領域を算出
	int x1 = __max(0, po.x);
	int y1 = __max(0, po.y);
	int x2 = __min(src.cols, po.x + objRGB.cols);
	int y2 = __min(src.rows, po.y + objRGB.rows);
	if (x1 >= x2 || y1 >= y2) return;

	cv::Rect dstROI(x1, y1, x2 - x1, y2 - y1);
	cv::Rect objROI(x1 - po.x, y1 - po.y, x2 - x1, y2 - y1);

	// アルファを3chに拡張してfloat[0,1]に変換
	cv::Mat alpha3ch;
	cv::cvtColor(objA(objROI), alpha3ch, cv::COLOR_GRAY2BGR);
	alpha3ch.convertTo(alpha3ch, CV_32FC3, 1.0 / 255.0);

	// ブレンド: dst = fg * alpha + bg * (1 - alpha)
	cv::Mat fg, bg;
	objRGB(objROI).convertTo(fg, CV_32FC3);
	dst(dstROI).convertTo(bg, CV_32FC3);

	cv::Mat result = fg.mul(alpha3ch) + bg.mul(cv::Scalar(1, 1, 1) - alpha3ch);
	result.convertTo(dst(dstROI), CV_8UC3);
}

// 処理時間計測
void measureTime(int i, char* pcMessage)
{
	static LARGE_INTEGER liFreq;
	static LARGE_INTEGER liStart;
	static LARGE_INTEGER liEnd;
	DWORD dwTime;

	switch (i) {
	case 0:
		// 計測開始
		QueryPerformanceFrequency(&liFreq);
		QueryPerformanceCounter(&liStart);
		break;
	case 1:
		// 計測終了
		QueryPerformanceCounter(&liEnd);
		dwTime = (DWORD)((liEnd.QuadPart - liStart.QuadPart) * 1000 / liFreq.QuadPart);
		printf("%s : %d[ms]\n", pcMessage, dwTime);
		break;
	}
}