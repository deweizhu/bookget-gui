// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "framework.h"
#include "Tab.h"

#define DOWNLOAD_TIMER_ID 1001
#define DOWNLOAD_DELAY_MS 1000*60  // 10���ӳ�
// �Զ�����Ϣ����
#define WM_APP_DOWNLOAD_COMPLETE (WM_APP + 1)  // �Զ������������Ϣ
#define WM_APP_DOWNLOAD_NEXT (WM_APP + 2)

class BrowserWindow
{
public:
    ~BrowserWindow();
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



// ͼƬ�������
private:
    bool IsInImageDownloadMode = false; //�Ƿ���ͼƬ��������
    std::vector<std::wstring> m_imageUrls; // �洢ͼƬURL�б�
    int m_downloadCounter = 1; // ���ؼ�����
    int m_currentDownloadIndex = 0; // ��ǰ��������
    wil::com_ptr<ICoreWebView2DownloadOperation> m_downloadOperation; // ���ز�������
    EventRegistrationToken m_downloadStartingToken; // ���ؿ�ʼ�¼�token
    EventRegistrationToken m_downloadStateChangedToken;  // �����������


    void LoadImageUrlsFromFile();
    void SetupDownloadHandler();
    std::wstring GetNextDownloadFilename();
    void DownloadNextImage();
    void StartDownloadProcess();
    void DownloadNextImageWithNavigate();
    void TriggerDownload(ICoreWebView2* webview);

private:
    // �����ڴ����
    HANDLE m_hSharedMemory = nullptr;
    LPVOID m_pSharedMemory = nullptr;
    const wchar_t* m_sharedMemoryName = L"Local\\WebView2SharedMemory";
    // ��ȷ����ṹ���С
    constexpr DWORD CalculateSharedMemorySize() {
         return sizeof(SharedMemoryData);  
    }

    const DWORD m_sharedMemorySize = CalculateSharedMemorySize();

    // �����ڴ滥����
    HANDLE m_hSharedMemoryMutex = nullptr;
    const wchar_t* m_sharedMemoryMutexName = L"Local\\WebView2SharedMemoryMutex";

    // �����ڴ�ṹ
    #pragma pack(push, 1)  // ȷ��������ֽ�
    struct SharedMemoryData {
        uint32_t URLReady;
        uint32_t HTMLReady;
        uint32_t CookiesReady;
        uint32_t PID; // ��ӽ���ID��ʶ
        wchar_t URL[1024];  // URL��󳤶�Ϊ1024�ַ�
        wchar_t HTML[1024 * 1024 * 10];  // 10MB HTML������
        wchar_t cookies[1024 * 10];  // 10KB Cookie������
    };
    #pragma pack(pop)  // �ָ�Ĭ�϶���

    bool InitSharedMemory();
    void ReadFromSharedMemory();
    void WriteHtmlToSharedMemory(const std::wstring& html);
    void CleanupSharedMemory();

public:
    void WriteCookiesToSharedMemory(const std::wstring& cookies);

};

