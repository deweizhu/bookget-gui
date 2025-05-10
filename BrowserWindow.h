// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "framework.h"
#include "Tab.h"

#define DOWNLOAD_TIMER_ID 1001
#define DOWNLOAD_DELAY_MS 1000*60  // 10秒延迟
// 自定义消息定义
#define WM_APP_DOWNLOAD_COMPLETE (WM_APP + 1)  // 自定义下载完成消息
#define WM_APP_DOWNLOAD_NEXT (WM_APP + 2)

class BrowserWindow
{
public:
    static const int c_uiBarHeight = 70;
    static const int c_optionsDropdownHeight = 108;
    static const int c_optionsDropdownWidth = 200;

    static ATOM RegisterClass(_In_ HINSTANCE hInstance);
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    static BOOL LaunchWindow(_In_ HINSTANCE hInstance, _In_ int nCmdShow);
    static std::wstring GetAppDataDirectory();
    std::wstring GetFullPathFor(LPCWSTR relativePath);
    HRESULT HandleTabURIUpdate(size_t tabId, ICoreWebView2* webview);
    HRESULT HandleTabHistoryUpdate(size_t tabId, ICoreWebView2* webview);
    HRESULT HandleTabNavStarting(size_t tabId, ICoreWebView2* webview);
    HRESULT HandleTabNavCompleted(size_t tabId, ICoreWebView2* webview, ICoreWebView2NavigationCompletedEventArgs* args);
    HRESULT HandleTabSecurityUpdate(size_t tabId, ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args);
    void HandleTabCreated(size_t tabId, bool shouldBeActive);
    HRESULT HandleTabMessageReceived(size_t tabId, ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* eventArgs);
    int GetDPIAwareBound(int bound);
    static void CheckFailure(HRESULT hr, LPCWSTR errorMessage);

    static std::wstring GetUserDataDirectory();

protected:
    HINSTANCE m_hInst = nullptr;  // Current app instance
    HWND m_hWnd = nullptr;

    static WCHAR s_windowClass[MAX_LOADSTRING];  // The window class name
    static WCHAR s_title[MAX_LOADSTRING];  // The title bar text

    int m_minWindowWidth = 0;
    int m_minWindowHeight = 0;

    wil::com_ptr<ICoreWebView2Environment> m_uiEnv;
    wil::com_ptr<ICoreWebView2Environment> m_contentEnv;
    wil::com_ptr<ICoreWebView2Controller> m_controlsController;
    wil::com_ptr<ICoreWebView2Controller> m_optionsController;
    wil::com_ptr<ICoreWebView2> m_controlsWebView;
    wil::com_ptr<ICoreWebView2> m_optionsWebView;
    std::map<size_t,std::unique_ptr<Tab>> m_tabs;
    size_t m_activeTabId = 0;

    EventRegistrationToken m_controlsUIMessageBrokerToken = {};  // Token for the UI message handler in controls WebView
    EventRegistrationToken m_controlsZoomToken = {};
    EventRegistrationToken m_optionsUIMessageBrokerToken = {};  // Token for the UI message handler in options WebView
    EventRegistrationToken m_optionsZoomToken = {};
    EventRegistrationToken m_lostOptionsFocus = {};  // Token for the lost focus handler in options WebView
    wil::com_ptr<ICoreWebView2WebMessageReceivedEventHandler> m_uiMessageBroker;

    BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
    HRESULT InitUIWebViews();
    HRESULT CreateBrowserControlsWebView();
    void CreateInitialTab();
    HRESULT CreateBrowserOptionsWebView();
    HRESULT ClearContentCache();
    HRESULT ClearControlsCache();
    HRESULT ClearContentCookies();
    HRESULT ClearControlsCookies();

    void SetUIMessageBroker();
    HRESULT ResizeUIWebViews();
    void UpdateMinWindowSize();
    HRESULT PostJsonToWebView(web::json::value jsonObj, ICoreWebView2* webview);
    HRESULT SwitchToTab(size_t tabId);
    std::wstring GetFilePathAsURI(std::wstring fullPath);



// BrowserWindow.h
private:
    bool IsInImageDownloadMode = false; //是否处于图片批量下载
    std::vector<std::wstring> m_imageUrls; // 存储图片URL列表
    int m_downloadCounter = 1; // 下载计数器
    int m_currentDownloadIndex = 0; // 当前下载索引
    wil::com_ptr<ICoreWebView2DownloadOperation> m_downloadOperation; // 下载操作对象
    EventRegistrationToken m_downloadStartingToken; // 下载开始事件token
    EventRegistrationToken m_downloadStateChangedToken;  // 添加这行声明


    void LoadImageUrlsFromFile();
    void SetupDownloadHandler();
    std::wstring GetNextDownloadFilename();
    void DownloadNextImage();
    void StartDownloadProcess();
    void DownloadNextImageWithNavigate();
    void TriggerDownload(ICoreWebView2* webview);

};

