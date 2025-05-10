// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BrowserWindow.h"
#include "shlobj.h"
#include <Urlmon.h>
#pragma comment (lib, "Urlmon.lib")
#include "Util.h"
#include "env.h"

// 在文件顶部添加包含
#include <fstream>
#include <sstream>
#include <shlwapi.h> // for PathCombine
#pragma comment(lib, "shlwapi.lib")

#include <filesystem>
#include <iostream>




using namespace Microsoft::WRL;
using Microsoft::WRL::Callback;

WCHAR BrowserWindow::s_windowClass[] = { 0 };
WCHAR BrowserWindow::s_title[] = { 0 };

//
//  FUNCTION: RegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM BrowserWindow::RegisterClass(_In_ HINSTANCE hInstance)
{
    // Initialize window class string
    LoadStringW(hInstance, IDC_BOOKGETAPP, s_windowClass, MAX_LOADSTRING);
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProcStatic;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WEBVIEWBROWSERAPP));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BOOKGETAPP);
    wcex.lpszClassName = s_windowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//  FUNCTION: WndProcStatic(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Redirect messages to approriate instance or call default proc
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK BrowserWindow::WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Get the ptr to the BrowserWindow instance who created this hWnd.
    // The pointer was set when the hWnd was created during InitInstance.
    BrowserWindow* browser_window = reinterpret_cast<BrowserWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (browser_window != nullptr)
    {
        return browser_window->WndProc(hWnd, message, wParam, lParam);  // Forward message to instance-aware WndProc
    }
    else
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for each browser window instance.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK BrowserWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

  switch (message)
    {
        case WM_APP_DOWNLOAD_NEXT:
        {
            m_currentDownloadIndex++;
            DownloadNextImageWithNavigate();  // 确保调用正确的函数名
        }
        break;
    
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* minmax = reinterpret_cast<MINMAXINFO*>(lParam);
            minmax->ptMinTrackSize.x = m_minWindowWidth;
            minmax->ptMinTrackSize.y = m_minWindowHeight;
        }
        break;
    
        case WM_DPICHANGED:
        {
            UpdateMinWindowSize();
        }
        break;
    
        case WM_SIZE:
        {
            ResizeUIWebViews();
            if (m_tabs.find(m_activeTabId) != m_tabs.end())
            {
                m_tabs.at(m_activeTabId)->ResizeWebView();
            }
        }
        break;
        case WM_TIMER:
        {
            if (wParam == 1) // 共享内存检查定时器
            {
                ReadFromSharedMemory();
            }
        }
        break;
        
        case WM_CLOSE:
        {
            KillTimer(m_hWnd, 1);
            CleanupSharedMemory();

            web::json::value jsonObj = web::json::value::parse(L"{}");
            jsonObj[L"message"] = web::json::value(MG_CLOSE_WINDOW);
            jsonObj[L"args"] = web::json::value::parse(L"{}");

            CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.get()), L"Try again.");
        }
        break;
    
        case WM_NCDESTROY:
        {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
            delete this;
            PostQuitMessage(0);
            return 0;  // 这里直接返回，不需要break
        }
    
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    
        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    return 0;
}


BOOL BrowserWindow::LaunchWindow(_In_ HINSTANCE hInstance, _In_ int nCmdShow)
{
    // BrowserWindow keeps a reference to itself in its host window and will
    // delete itself when the window is destroyed.
    BrowserWindow* window = new BrowserWindow();
    if (!window->InitInstance(hInstance, nCmdShow))
    {
        delete window;
        return FALSE;
    }
    return TRUE;
}

// BrowserWindow.cpp
BrowserWindow::~BrowserWindow()
{
    CleanupSharedMemory();
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL BrowserWindow::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    m_hInst = hInstance; // Store app instance handle
    LoadStringW(m_hInst, IDS_APP_TITLE, s_title, MAX_LOADSTRING);

    // 初始化共享内存
    if (InitSharedMemory())
    {
        ReadFromSharedMemory();
    }

    SetUIMessageBroker();

    m_hWnd = CreateWindowW(s_windowClass, s_title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, m_hInst, nullptr);

    if (!m_hWnd)
    {
        return FALSE;
    }

     // 设置定时器定期检查共享内存
     SetTimer(m_hWnd, 1, 100, NULL);

    // Make the BrowserWindow instance ptr available through the hWnd
    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    UpdateMinWindowSize();
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    // Get directory for user data. This will be kept separated from the
    // directory for the browser UI data.
    std::wstring userDataDirectory = GetUserDataDirectory();

    // Create WebView environment for web content requested by the user. All
    // tabs will be created from this environment and kept isolated from the
    // browser UI. This enviroment is created first so the UI can request new
    // tabs when it's ready.
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataDirectory.c_str(),
        nullptr, Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
    {
        RETURN_IF_FAILED(result);

        m_contentEnv = env;
        HRESULT hr = InitUIWebViews();

        if (!SUCCEEDED(hr))
        {
            OutputDebugString(L"UI WebViews environment creation failed\n");
        }

        if (Util::CheckIfUrlsFileExists()) 
        {
            IsInImageDownloadMode = true;
         // 设置定时器，等待WebView完全初始化
            SetTimer(m_hWnd, 2, 1000*10, [](HWND hWnd, UINT, UINT_PTR, DWORD) {
                KillTimer(hWnd, 2);
                BrowserWindow* window = reinterpret_cast<BrowserWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                if (window) {
                    window->StartDownloadProcess();
                }
            });
        }

        return hr;
    }).Get());

    if (!SUCCEEDED(hr))
    {
        OutputDebugString(L"Content WebViews environment creation failed\n");
        return FALSE;
    }

    return TRUE;
}

HRESULT BrowserWindow::InitUIWebViews()
{
    // Get data directory for browser UI data
    std::wstring browserDataDirectory = GetAppDataDirectory();
    browserDataDirectory.append(L"\\Browser Data");

    // Create WebView environment for browser UI. A separate data directory is
    // used to isolate the browser UI from web content requested by the user.
    return CreateCoreWebView2EnvironmentWithOptions(nullptr, browserDataDirectory.c_str(),
        nullptr, Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
    {
        // Environment is ready, create the WebView
        m_uiEnv = env;

        RETURN_IF_FAILED(CreateBrowserControlsWebView());
        RETURN_IF_FAILED(CreateBrowserOptionsWebView());

        return S_OK;
    }).Get());
}

HRESULT BrowserWindow::CreateBrowserControlsWebView()
{
    return m_uiEnv->CreateCoreWebView2Controller(m_hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [this](HRESULT result, ICoreWebView2Controller* host) -> HRESULT
    {
        if (!SUCCEEDED(result))
        {
            OutputDebugString(L"Controls WebView creation failed\n");
            return result;
        }
        // WebView created
        m_controlsController = host;
        CheckFailure(m_controlsController->get_CoreWebView2(&m_controlsWebView), L"");

        wil::com_ptr<ICoreWebView2Settings> settings;
        RETURN_IF_FAILED(m_controlsWebView->get_Settings(&settings));
        RETURN_IF_FAILED(settings->put_AreDevToolsEnabled(FALSE));

        // 禁用弹窗
        RETURN_IF_FAILED(settings->put_AreDefaultScriptDialogsEnabled(FALSE));

        // 设置新窗口在当前标签页打开
        RETURN_IF_FAILED(m_controlsWebView->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
        {
            // 获取请求的URI
            wil::unique_cotaskmem_string uri;
            args->get_Uri(&uri);
            
            // 在当前WebView中导航到该URI
            //m_controlsWebView->Navigate(uri.get());

             // 创建新标签页
            web::json::value jsonObj = web::json::value::parse(L"{}");
            jsonObj[L"message"] = web::json::value(MG_CREATE_TAB);
            jsonObj[L"args"] = web::json::value::parse(L"{}");
            jsonObj[L"args"][L"tabId"] = web::json::value::number(m_tabs.size()); // 使用下一个可用ID
            jsonObj[L"args"][L"active"] = web::json::value::boolean(true);
            jsonObj[L"args"][L"uri"] = web::json::value(uri.get());
    
            // 发送消息创建新标签页并导航
            PostJsonToWebView(jsonObj, m_controlsWebView.get());
    
            // 取消默认的新窗口行为
            args->put_Handled(TRUE);
      
            
            return S_OK;
        }).Get(), &m_newWindowRequestedToken));

        RETURN_IF_FAILED(m_controlsController->add_ZoomFactorChanged(Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
            [](ICoreWebView2Controller* host, IUnknown* args) -> HRESULT
        {
            host->put_ZoomFactor(1.0);
            return S_OK;
        }
        ).Get(), &m_controlsZoomToken));

        RETURN_IF_FAILED(m_controlsWebView->add_WebMessageReceived(m_uiMessageBroker.get(), &m_controlsUIMessageBrokerToken));
        RETURN_IF_FAILED(ResizeUIWebViews());

        std::wstring controlsPath = GetFullPathFor(L"gui\\controls_ui\\default.html");
        RETURN_IF_FAILED(m_controlsWebView->Navigate(controlsPath.c_str()));

         // 在这里创建第一个标签页
        //CreateInitialTab();


        

        return S_OK;
    }).Get());
}

void BrowserWindow::CreateInitialTab()
{
    if (!m_controlsWebView) {
        OutputDebugString(L"Controls WebView not ready\n");
        return;
    }

    web::json::value jsonObj = web::json::value::parse(L"{}");
    jsonObj[L"message"] = web::json::value(MG_CREATE_TAB);
    jsonObj[L"args"] = web::json::value::parse(L"{}");
    jsonObj[L"args"][L"tabId"] = web::json::value::number(0);
    jsonObj[L"args"][L"active"] = web::json::value::boolean(true);
    
    PostJsonToWebView(jsonObj, m_controlsWebView.get());
}

HRESULT BrowserWindow::CreateBrowserOptionsWebView()
{
    return m_uiEnv->CreateCoreWebView2Controller(m_hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [this](HRESULT result, ICoreWebView2Controller* host) -> HRESULT
    {
        if (!SUCCEEDED(result))
        {
            OutputDebugString(L"Options WebView creation failed\n");
            return result;
        }
        // WebView created
        m_optionsController = host;
        CheckFailure(m_optionsController->get_CoreWebView2(&m_optionsWebView), L"");

        wil::com_ptr<ICoreWebView2Settings> settings;
        RETURN_IF_FAILED(m_optionsWebView->get_Settings(&settings));
        RETURN_IF_FAILED(settings->put_AreDevToolsEnabled(FALSE));

         // 禁用弹窗
        RETURN_IF_FAILED(settings->put_AreDefaultScriptDialogsEnabled(FALSE));
        
        // 设置新窗口在当前标签页打开
        RETURN_IF_FAILED(m_optionsWebView->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
        {
            // 获取请求的URI
            wil::unique_cotaskmem_string uri;
            args->get_Uri(&uri);
            
            // 创建新标签页
            web::json::value jsonObj = web::json::value::parse(L"{}");
            jsonObj[L"message"] = web::json::value(MG_CREATE_TAB);
            jsonObj[L"args"] = web::json::value::parse(L"{}");
            jsonObj[L"args"][L"tabId"] = web::json::value::number(m_tabs.size()); // 使用下一个可用ID
            jsonObj[L"args"][L"active"] = web::json::value::boolean(true);
            jsonObj[L"args"][L"uri"] = web::json::value(uri.get());
    
            // 发送消息创建新标签页并导航
            PostJsonToWebView(jsonObj, m_controlsWebView.get());
    
            // 取消默认的新窗口行为
            args->put_Handled(TRUE);
            
            return S_OK;
        }).Get(), &m_newWindowRequestedToken));

        RETURN_IF_FAILED(m_optionsController->add_ZoomFactorChanged(Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
            [](ICoreWebView2Controller* host, IUnknown* args) -> HRESULT
        {
            host->put_ZoomFactor(1.0);
            return S_OK;
        }
        ).Get(), &m_optionsZoomToken));

        // Hide by default
        RETURN_IF_FAILED(m_optionsController->put_IsVisible(FALSE));
        RETURN_IF_FAILED(m_optionsWebView->add_WebMessageReceived(m_uiMessageBroker.get(), &m_optionsUIMessageBrokerToken));

        // Hide menu when focus is lost
        RETURN_IF_FAILED(m_optionsController->add_LostFocus(Callback<ICoreWebView2FocusChangedEventHandler>(
            [this](ICoreWebView2Controller* sender, IUnknown* args) -> HRESULT
        {
            web::json::value jsonObj = web::json::value::parse(L"{}");
            jsonObj[L"message"] = web::json::value(MG_OPTIONS_LOST_FOCUS);
            jsonObj[L"args"] = web::json::value::parse(L"{}");

            PostJsonToWebView(jsonObj, m_controlsWebView.get());

            return S_OK;
        }).Get(), &m_lostOptionsFocus));

        RETURN_IF_FAILED(ResizeUIWebViews());

        std::wstring optionsPath = GetFullPathFor(L"gui\\controls_ui\\options.html");
        RETURN_IF_FAILED(m_optionsWebView->Navigate(optionsPath.c_str()));

        return S_OK;
    }).Get());
}

// Set the message broker for the UI webview. This will capture messages from ui web content.
// Lambda is used to capture the instance while satisfying Microsoft::WRL::Callback<T>()
void BrowserWindow::SetUIMessageBroker()
{
    m_uiMessageBroker = Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [this](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* eventArgs) -> HRESULT
    {
        wil::unique_cotaskmem_string jsonString;
        CheckFailure(eventArgs->get_WebMessageAsJson(&jsonString), L"");  // Get the message from the UI WebView as JSON formatted string
        web::json::value jsonObj = web::json::value::parse(jsonString.get());

        if (!jsonObj.has_field(L"message"))
        {
            OutputDebugString(L"No message code provided\n");
            return S_OK;
        }

        if (!jsonObj.has_field(L"args"))
        {
            OutputDebugString(L"The message has no args field\n");
            return S_OK;
        }

        int message = jsonObj.at(L"message").as_integer();
        web::json::value args = jsonObj.at(L"args");

        switch (message)
        {
        case MG_CREATE_TAB:
        {
            size_t id = args.at(L"tabId").as_number().to_uint32();
            bool shouldBeActive = args.at(L"active").as_bool();
            std::unique_ptr<Tab> newTab = Tab::CreateNewTab(m_hWnd, m_contentEnv.get(), id, shouldBeActive);

            // 如果有提供URI，在新标签页中导航
            if (args.has_field(L"uri"))
            {
                std::wstring uri = args.at(L"uri").as_string();
                newTab->m_contentWebView->Navigate(uri.c_str());
            }

            std::map<size_t, std::unique_ptr<Tab>>::iterator it = m_tabs.find(id);
            if (it == m_tabs.end())
            {
                m_tabs.insert(std::pair<size_t,std::unique_ptr<Tab>>(id, std::move(newTab)));
            }
            else
            {
                m_tabs.at(id)->m_contentController->Close();
                it->second = std::move(newTab);
            }
        }
        break;
        case MG_NAVIGATE:
        {
            std::wstring uri(args.at(L"uri").as_string());
            std::wstring browserScheme(L"browser://");

            if (uri.substr(0, browserScheme.size()).compare(browserScheme) == 0)
            {
                // No encoded search URI
                std::wstring path = uri.substr(browserScheme.size());
                if (path.compare(L"favorites") == 0 ||
                    path.compare(L"settings") == 0 ||
                    path.compare(L"history") == 0)
                {
                    std::wstring filePath(L"gui\\content_ui\\");
                    filePath.append(path);
                    filePath.append(L".html");
                    std::wstring fullPath = GetFullPathFor(filePath.c_str());
                    CheckFailure(m_tabs.at(m_activeTabId)->m_contentWebView->Navigate(fullPath.c_str()), L"Can't navigate to browser page.");
                }
                else
                {
                    OutputDebugString(L"Requested unknown browser page\n");
                }
            }
            else if (!SUCCEEDED(m_tabs.at(m_activeTabId)->m_contentWebView->Navigate(uri.c_str())))
            {
                CheckFailure(m_tabs.at(m_activeTabId)->m_contentWebView->Navigate(args.at(L"encodedSearchURI").as_string().c_str()), L"Can't navigate to requested page.");
            }
        }
        break;
        case MG_GO_FORWARD:
        {
            CheckFailure(m_tabs.at(m_activeTabId)->m_contentWebView->GoForward(), L"");
        }
        break;
        case MG_GO_BACK:
        {
            CheckFailure(m_tabs.at(m_activeTabId)->m_contentWebView->GoBack(), L"");
        }
        break;
        case MG_RELOAD:
        {
            CheckFailure(m_tabs.at(m_activeTabId)->m_contentWebView->Reload(), L"");
             
            //! [CookieManager]
            wil::unique_cotaskmem_string source;
            RETURN_IF_FAILED(m_tabs.at(m_activeTabId)->m_contentWebView->get_Source(&source));
            std::wstring uri(source.get());
            CheckFailure(m_tabs.at(m_activeTabId)->GetCookies(uri), L"");
        }
        break;
        case MG_CANCEL:
        {
            CheckFailure(m_tabs.at(m_activeTabId)->m_contentWebView->CallDevToolsProtocolMethod(L"Page.stopLoading", L"{}", nullptr), L"");
        }
        break;
        case MG_SWITCH_TAB:
        {
            size_t tabId = args.at(L"tabId").as_number().to_uint32();

            SwitchToTab(tabId);
        }
        break;
        case MG_CLOSE_TAB:
        {
            size_t id = args.at(L"tabId").as_number().to_uint32();
            m_tabs.at(id)->m_contentController->Close();
            m_tabs.erase(id);
        }
        break;
        case MG_CLOSE_WINDOW:
        {
            DestroyWindow(m_hWnd);
        }
        break;
        case MG_SHOW_OPTIONS:
        {
            CheckFailure(m_optionsController->put_IsVisible(TRUE), L"");
            m_optionsController->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
        break;
        case MG_HIDE_OPTIONS:
        {
            CheckFailure(m_optionsController->put_IsVisible(FALSE), L"Something went wrong when trying to close the options dropdown.");
        }
        break;
        case MG_OPTION_SELECTED:
        {
            m_tabs.at(m_activeTabId)->m_contentController->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
        break;
        case MG_GET_FAVORITES:
        case MG_GET_SETTINGS:
        case MG_GET_HISTORY:
        {
            // Forward back to requesting tab
            size_t tabId = args.at(L"tabId").as_number().to_uint32();
            jsonObj[L"args"].erase(L"tabId");

            CheckFailure(PostJsonToWebView(jsonObj, m_tabs.at(tabId)->m_contentWebView.get()), L"Requesting history failed.");
        }
        break;
        default:
        {
            OutputDebugString(L"Unexpected message\n");
        }
        break;
        }

        return S_OK;
    });
}

HRESULT BrowserWindow::SwitchToTab(size_t tabId)
{
    // 检查标签页是否存在
    if (m_tabs.find(tabId) == m_tabs.end())
    {
        return E_INVALIDARG;
    }

    size_t previousActiveTab = m_activeTabId;

    // 激活新标签页
    RETURN_IF_FAILED(m_tabs.at(tabId)->ResizeWebView());
    RETURN_IF_FAILED(m_tabs.at(tabId)->m_contentController->put_IsVisible(TRUE));
    m_activeTabId = tabId;

    // 隐藏之前的活动标签页
    if (previousActiveTab != INVALID_TAB_ID && previousActiveTab != m_activeTabId) 
    {
        auto previousTabIterator = m_tabs.find(previousActiveTab);
        if (previousTabIterator != m_tabs.end() && previousTabIterator->second &&
            previousTabIterator->second->m_contentController)
        {
            previousTabIterator->second->m_contentController->put_IsVisible(FALSE);
        }
    }

    return S_OK;
}

HRESULT BrowserWindow::HandleTabURIUpdate(size_t tabId, ICoreWebView2* webview)
{
    wil::unique_cotaskmem_string source;
    RETURN_IF_FAILED(webview->get_Source(&source));

    web::json::value jsonObj = web::json::value::parse(L"{}");
    jsonObj[L"message"] = web::json::value(MG_UPDATE_URI);
    jsonObj[L"args"] = web::json::value::parse(L"{}");
    jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);
    jsonObj[L"args"][L"uri"] = web::json::value(source.get());

    std::wstring uri(source.get());
    std::wstring favoritesURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\favorites.html"));
    std::wstring settingsURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\settings.html"));
    std::wstring historyURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\history.html"));

    if (uri.compare(favoritesURI) == 0)
    {
        jsonObj[L"args"][L"uriToShow"] = web::json::value(L"browser://favorites");
    }
    else if (uri.compare(settingsURI) == 0)
    {
        jsonObj[L"args"][L"uriToShow"] = web::json::value(L"browser://settings");
    }
    else if (uri.compare(historyURI) == 0)
    {
        jsonObj[L"args"][L"uriToShow"] = web::json::value(L"browser://history");
    }

    RETURN_IF_FAILED(PostJsonToWebView(jsonObj, m_controlsWebView.get()));


     //! [ Get Cookies ]
    if (g_cmd == L"-i") {
         wil::unique_cotaskmem_string source;
         RETURN_IF_FAILED(webview->get_Source(&source));
         std::wstring uri(source.get());
         CheckFailure(m_tabs.at(tabId)->GetCookies(uri), L"");
    }
    //! [Get HTML File]
     if (!g_outHtmlFile.empty()) {
         std::wstring getSourceHtml(
             L"(() => {"
             // html body
             L"    return document.documentElement ? document.documentElement.outerHTML : document.body;"
             L"})();"
         );
         CheckFailure(webview->ExecuteScript(getSourceHtml.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
         [this, tabId](HRESULT error, PCWSTR result) -> HRESULT
         {
             RETURN_IF_FAILED(error);
             web::json::value jsonObj = web::json::value::parse(L"{}");
             jsonObj[L"html"]  = web::json::value::parse(result);
             Util::fileWrite(Util::GetUserHomeDirectory() + L"\\bookget\\" + g_outHtmlFile, jsonObj[L"html"].as_string());
             return S_OK;
         }).Get()), L"Can't update favicon");
     }
    return S_OK;
}

HRESULT BrowserWindow::HandleTabHistoryUpdate(size_t tabId, ICoreWebView2* webview)
{
    wil::unique_cotaskmem_string source;
    RETURN_IF_FAILED(webview->get_Source(&source));
    
    web::json::value jsonObj = web::json::value::parse(L"{}");
    jsonObj[L"message"] = web::json::value(MG_UPDATE_URI);
    jsonObj[L"args"] = web::json::value::parse(L"{}");
    jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);
    jsonObj[L"args"][L"uri"] = web::json::value(source.get());

    BOOL canGoForward = FALSE;
    RETURN_IF_FAILED(webview->get_CanGoForward(&canGoForward));
    jsonObj[L"args"][L"canGoForward"] = web::json::value::boolean(canGoForward);

    BOOL canGoBack = FALSE;
    RETURN_IF_FAILED(webview->get_CanGoBack(&canGoBack));
    jsonObj[L"args"][L"canGoBack"] = web::json::value::boolean(canGoBack);

    RETURN_IF_FAILED(PostJsonToWebView(jsonObj, m_controlsWebView.get()));

    return S_OK;
}

HRESULT BrowserWindow::HandleTabNavStarting(size_t tabId, ICoreWebView2* webview)
{
    web::json::value jsonObj = web::json::value::parse(L"{}");
    jsonObj[L"message"] = web::json::value(MG_NAV_STARTING);
    jsonObj[L"args"] = web::json::value::parse(L"{}");
    jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);

    return PostJsonToWebView(jsonObj, m_controlsWebView.get());
}

HRESULT BrowserWindow::HandleTabNavCompleted(size_t tabId, ICoreWebView2* webview, ICoreWebView2NavigationCompletedEventArgs* args)
{
    std::wstring getTitleScript(
        // Look for a title tag
        L"(() => {"
        L"    const titleTag = document.getElementsByTagName('title')[0];"
        L"    if (titleTag) {"
        L"        return titleTag.innerHTML;"
        L"    }"
        // No title tag, look for the file name
        L"    pathname = window.location.pathname;"
        L"    var filename = pathname.split('/').pop();"
        L"    if (filename) {"
        L"        return filename;"
        L"    }"
        // No file name, look for the hostname
        L"    const hostname =  window.location.hostname;"
        L"    if (hostname) {"
        L"        return hostname;"
        L"    }"
        // Fallback: let the UI use a generic title
        L"    return '';"
        L"})();"
    );

    std::wstring getFaviconURI(
        L"(() => {"
        // Let the UI use a fallback favicon
        L"    let faviconURI = '';"
        L"    let links = document.getElementsByTagName('link');"
        // Test each link for a favicon
        L"    Array.from(links).map(element => {"
        L"        let rel = element.rel;"
        // Favicon is declared, try to get the href
        L"        if (rel && (rel == 'shortcut icon' || rel == 'icon')) {"
        L"            if (!element.href) {"
        L"                return;"
        L"            }"
        // href to icon found, check it's full URI
        L"            try {"
        L"                let urlParser = new URL(element.href);"
        L"                faviconURI = urlParser.href;"
        L"            } catch(e) {"
        // Try prepending origin
        L"                let origin = window.location.origin;"
        L"                let faviconLocation = `${origin}/${element.href}`;"
        L"                try {"
        L"                    urlParser = new URL(faviconLocation);"
        L"                    faviconURI = urlParser.href;"
        L"                } catch (e2) {"
        L"                    return;"
        L"                }"
        L"            }"
        L"        }"
        L"    });"
        L"    return faviconURI;"
        L"})();"
    );

    CheckFailure(webview->ExecuteScript(getTitleScript.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this, tabId](HRESULT error, PCWSTR result) -> HRESULT
    {
        RETURN_IF_FAILED(error);

        web::json::value jsonObj = web::json::value::parse(L"{}");
        jsonObj[L"message"] = web::json::value(MG_UPDATE_TAB);
        jsonObj[L"args"] = web::json::value::parse(L"{}");
        jsonObj[L"args"][L"title"] = web::json::value::parse(result);
        jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);

        CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.get()), L"Can't update title.");
        return S_OK;
    }).Get()), L"Can't update title.");

    CheckFailure(webview->ExecuteScript(getFaviconURI.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this, tabId](HRESULT error, PCWSTR result) -> HRESULT
    {
        RETURN_IF_FAILED(error);

        web::json::value jsonObj = web::json::value::parse(L"{}");
        jsonObj[L"message"] = web::json::value(MG_UPDATE_FAVICON);
        jsonObj[L"args"] = web::json::value::parse(L"{}");
        jsonObj[L"args"][L"uri"] = web::json::value::parse(result);
        jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);

        CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.get()), L"Can't update favicon.");
        return S_OK;
    }).Get()), L"Can't update favicon");

    web::json::value jsonObj = web::json::value::parse(L"{}");
    jsonObj[L"message"] = web::json::value(MG_NAV_COMPLETED);
    jsonObj[L"args"] = web::json::value::parse(L"{}");
    jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);

    BOOL navigationSucceeded = FALSE;
    if (SUCCEEDED(args->get_IsSuccess(&navigationSucceeded)))
    {
        jsonObj[L"args"][L"isError"] = web::json::value::boolean(!navigationSucceeded);
    }
     // 检查是否处于图片下载模式
    if (IsInImageDownloadMode)
    {
        // 获取当前URL
         wil::unique_cotaskmem_string source;
         RETURN_IF_FAILED(webview->get_Source(&source));
        
         TriggerDownload(webview);
       
    }
    else {
         //! [ Get Cookies ]
      wil::unique_cotaskmem_string source;
      RETURN_IF_FAILED(webview->get_Source(&source));
      std::wstring uri(source.get());
      CheckFailure(m_tabs.at(tabId)->GetCookies(uri), L"");

         //! [Get HTML File]
        std::wstring getSourceHtml(
            L"(() => {"
            L"    return document.documentElement ? document.documentElement.outerHTML : document.body;"
            L"})();"
        );

        CheckFailure(webview->ExecuteScript(getSourceHtml.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this, tabId](HRESULT error, PCWSTR result) -> HRESULT
        {
            RETURN_IF_FAILED(error);
            web::json::value jsonObj = web::json::value::parse(L"{}");
            jsonObj[L"html"]  = web::json::value::parse(result);

            WriteHtmlToSharedMemory(jsonObj[L"html"].as_string());
            //Util::fileWrite(Util::GetUserHomeDirectory() + L"\\bookget\\"+ g_outHtmlFile, jsonObj[L"html"].as_string());
            return S_OK;
        }).Get()), L"Can't update favicon");
    }
     
    return PostJsonToWebView(jsonObj, m_controlsWebView.get());
}

HRESULT BrowserWindow::HandleTabSecurityUpdate(size_t tabId, ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args)
{
    wil::unique_cotaskmem_string jsonArgs;
    RETURN_IF_FAILED(args->get_ParameterObjectAsJson(&jsonArgs));
    web::json::value securityEvent = web::json::value::parse(jsonArgs.get());

    web::json::value jsonObj = web::json::value::parse(L"{}");
    jsonObj[L"message"] = web::json::value(MG_SECURITY_UPDATE);
    jsonObj[L"args"] = web::json::value::parse(L"{}");
    jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);
    jsonObj[L"args"][L"state"] = securityEvent.at(L"securityState");

    return PostJsonToWebView(jsonObj, m_controlsWebView.get());
}

void BrowserWindow::HandleTabCreated(size_t tabId, bool shouldBeActive)
{
    if (shouldBeActive)
    {
        CheckFailure(SwitchToTab(tabId), L"");
    }
}

HRESULT BrowserWindow::HandleTabMessageReceived(size_t tabId, ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* eventArgs)
{
    wil::unique_cotaskmem_string jsonString;
    RETURN_IF_FAILED(eventArgs->get_WebMessageAsJson(&jsonString));
    web::json::value jsonObj = web::json::value::parse(jsonString.get());

    wil::unique_cotaskmem_string uri;
    RETURN_IF_FAILED(webview->get_Source(&uri));

    int message = jsonObj.at(L"message").as_integer();
    web::json::value args = jsonObj.at(L"args");

    wil::unique_cotaskmem_string source;
    RETURN_IF_FAILED(webview->get_Source(&source));

    switch (message)
    {
    case MG_GET_FAVORITES:
    case MG_REMOVE_FAVORITE:
    {
        std::wstring fileURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\favorites.html"));
        // Only the favorites UI can request favorites
        if (fileURI.compare(source.get()) == 0)
        {
            jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);
            CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.get()), L"Couldn't perform favorites operation.");
        }
    }
    break;
    case MG_GET_SETTINGS:
    {
        std::wstring fileURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\settings.html"));
        // Only the settings UI can request settings
        if (fileURI.compare(source.get()) == 0)
        {
            jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);
            CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.get()), L"Couldn't retrieve settings.");
        }
    }
    break;
    case MG_CLEAR_CACHE:
    {
        std::wstring fileURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\settings.html"));
        // Only the settings UI can request cache clearing
        if (fileURI.compare(uri.get()) == 0)
        {
            jsonObj[L"args"][L"content"] = web::json::value::boolean(false);
            jsonObj[L"args"][L"controls"] = web::json::value::boolean(false);

            if (SUCCEEDED(ClearContentCache()))
            {
                jsonObj[L"args"][L"content"] = web::json::value::boolean(true);
            }

            if (SUCCEEDED(ClearControlsCache()))
            {
                jsonObj[L"args"][L"controls"] = web::json::value::boolean(true);
            }

            CheckFailure(PostJsonToWebView(jsonObj, m_tabs.at(tabId)->m_contentWebView.get()), L"");
        }
    }
    break;
    case MG_CLEAR_COOKIES:
    {
        std::wstring fileURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\settings.html"));
        // Only the settings UI can request cookies clearing
        if (fileURI.compare(uri.get()) == 0)
        {
            jsonObj[L"args"][L"content"] = web::json::value::boolean(false);
            jsonObj[L"args"][L"controls"] = web::json::value::boolean(false);

            if (SUCCEEDED(ClearContentCookies()))
            {
                jsonObj[L"args"][L"content"] = web::json::value::boolean(true);
            }


            if (SUCCEEDED(ClearControlsCookies()))
            {
                jsonObj[L"args"][L"controls"] = web::json::value::boolean(true);
            }

            CheckFailure(PostJsonToWebView(jsonObj, m_tabs.at(tabId)->m_contentWebView.get()), L"");
        }
    }
    break;
    case MG_GET_HISTORY:
    case MG_REMOVE_HISTORY_ITEM:
    case MG_CLEAR_HISTORY:
    {
        std::wstring fileURI = GetFilePathAsURI(GetFullPathFor(L"gui\\content_ui\\history.html"));
        // Only the history UI can request history
        if (fileURI.compare(uri.get()) == 0)
        {
            jsonObj[L"args"][L"tabId"] = web::json::value::number(tabId);
            CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.get()), L"Couldn't perform history operation");
        }
    }
    break;
    default:
    {
        OutputDebugString(L"Unexpected message\n");
    }
    break;
    }

    return S_OK;
}

HRESULT BrowserWindow::ClearContentCache()
{
    return m_tabs.at(m_activeTabId)->m_contentWebView->CallDevToolsProtocolMethod(L"Network.clearBrowserCache", L"{}", nullptr);
}

HRESULT BrowserWindow::ClearControlsCache()
{
    return m_controlsWebView->CallDevToolsProtocolMethod(L"Network.clearBrowserCache", L"{}", nullptr);
}

HRESULT BrowserWindow::ClearContentCookies()
{
    return m_tabs.at(m_activeTabId)->m_contentWebView->CallDevToolsProtocolMethod(L"Network.clearBrowserCookies", L"{}", nullptr);
}

HRESULT BrowserWindow::ClearControlsCookies()
{
    return m_controlsWebView->CallDevToolsProtocolMethod(L"Network.clearBrowserCookies", L"{}", nullptr);
}

HRESULT BrowserWindow::ResizeUIWebViews()
{
    if (m_controlsWebView != nullptr)
    {
        RECT bounds;
        GetClientRect(m_hWnd, &bounds);
        bounds.bottom = bounds.top + GetDPIAwareBound(c_uiBarHeight);
        bounds.bottom += 1;

        RETURN_IF_FAILED(m_controlsController->put_Bounds(bounds));
    }

    if (m_optionsWebView != nullptr)
    {
        RECT bounds;
        GetClientRect(m_hWnd, &bounds);
        bounds.top = GetDPIAwareBound(c_uiBarHeight);
        bounds.bottom = bounds.top + GetDPIAwareBound(c_optionsDropdownHeight);
        bounds.left = bounds.right - GetDPIAwareBound(c_optionsDropdownWidth);

        RETURN_IF_FAILED(m_optionsController->put_Bounds(bounds));
    }

    // Workaround for black controls WebView issue in Windows 7
    HWND wvWindow = GetWindow(m_hWnd, GW_CHILD);
    while (wvWindow != nullptr)
    {
        UpdateWindow(wvWindow);
        wvWindow = GetWindow(wvWindow, GW_HWNDNEXT);
    }

    return S_OK;
}

void BrowserWindow::UpdateMinWindowSize()
{
    RECT clientRect;
    RECT windowRect;

    GetClientRect(m_hWnd, &clientRect);
    GetWindowRect(m_hWnd, &windowRect);

    int bordersWidth = (windowRect.right - windowRect.left) - clientRect.right;
    int bordersHeight = (windowRect.bottom - windowRect.top) - clientRect.bottom;

    m_minWindowWidth = GetDPIAwareBound(MIN_WINDOW_WIDTH) + bordersWidth;
    m_minWindowHeight = GetDPIAwareBound(MIN_WINDOW_HEIGHT) + bordersHeight;
}

void BrowserWindow::CheckFailure(HRESULT hr, LPCWSTR errorMessage)
{
    if (FAILED(hr))
    {
        std::wstring message;
        if (!errorMessage || !errorMessage[0])
        {
            message = std::wstring(L"Something went wrong.");
        }
        else
        {
            message = std::wstring(errorMessage);
        }

        MessageBoxW(nullptr, message.c_str(), nullptr, MB_OK);
    }
}

int BrowserWindow::GetDPIAwareBound(int bound)
{
    // Remove the GetDpiForWindow call when using Windows 7 or any version
    // below 1607 (Windows 10). You will also have to make sure the build
    // directory is clean before building again.
    return (bound * GetDpiForWindow(m_hWnd) / DEFAULT_DPI);
}

std::wstring BrowserWindow::GetAppDataDirectory()
{
    TCHAR path[MAX_PATH];
    std::wstring dataDirectory;
    HRESULT hr = SHGetFolderPath(nullptr, CSIDL_APPDATA, NULL, 0, path);
    if (SUCCEEDED(hr))
    {
        dataDirectory = std::wstring(path);
        dataDirectory.append(L"\\Bookget\\");
    }
    else
    {
        dataDirectory = std::wstring(L".\\");
    }

    //dataDirectory.append(s_title);
    return dataDirectory;
}

std::wstring BrowserWindow::GetFullPathFor(LPCWSTR relativePath)
{
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(m_hInst, path, MAX_PATH);
    std::wstring pathName(path);

    std::size_t index = pathName.find_last_of(L"\\") + 1;
    pathName.replace(index, pathName.length(), relativePath);

    return pathName;
}

std::wstring BrowserWindow::GetFilePathAsURI(std::wstring fullPath)
{
    std::wstring fileURI;
    ComPtr<IUri> uri;
    DWORD uriFlags = Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME;
    HRESULT hr = CreateUri(fullPath.c_str(), uriFlags, 0, &uri);

    if (SUCCEEDED(hr))
    {
        wil::unique_bstr absoluteUri;
        uri->GetAbsoluteUri(&absoluteUri);
        fileURI = std::wstring(absoluteUri.get());
    }

    return fileURI;
}

HRESULT BrowserWindow::PostJsonToWebView(web::json::value jsonObj, ICoreWebView2* webview)
{
    utility::stringstream_t stream;
    jsonObj.serialize(stream);

    return webview->PostWebMessageAsJson(stream.str().c_str());
}

std::wstring BrowserWindow::GetUserDataDirectory() {
    std::wstring userDataDirectory = GetAppDataDirectory();
    userDataDirectory.append(L"\\User Data");
    return userDataDirectory;
}


void BrowserWindow::StartDownloadProcess()
{
    // 确保downloads目录存在
    std::wstring downloadsDir = Util::GetCurrentExeDirectory() + L"\\downloads";
    if (!CreateDirectory(downloadsDir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        OutputDebugString(L"Could not create downloads directory\n");
        return;
    }

    // 重置计数器
    m_downloadCounter = 1;
    m_currentDownloadIndex = 0;

    // 加载URL列表
    LoadImageUrlsFromFile();

    if (!m_imageUrls.empty())
    {
        // 设置下载处理器
        SetupDownloadHandler();
        
        // 开始第一个下载
        DownloadNextImage();
    }
}

void BrowserWindow::DownloadNextImage()
{
    if (m_currentDownloadIndex >= m_imageUrls.size())
    {
        OutputDebugString(L"All downloads completed\n");
        return;
    }

    if (m_tabs.find(m_activeTabId) != m_tabs.end() && m_tabs.at(m_activeTabId)->m_contentWebView)
    {
        OutputDebugString(L"Downloading: ");
        OutputDebugString(m_imageUrls[m_currentDownloadIndex].c_str());
        OutputDebugString(L"\n");
        
        // 导航到图片URL，这将触发下载
        m_tabs.at(m_activeTabId)->m_contentWebView->Navigate(m_imageUrls[m_currentDownloadIndex].c_str());
    }
}

std::wstring BrowserWindow::GetNextDownloadFilename()
{
    std::wstringstream filename;
    filename << std::setw(4) << std::setfill(L'0') << m_downloadCounter++;
    
    // 尝试从URL获取文件扩展名
    size_t dotPos = m_imageUrls[m_currentDownloadIndex].find_last_of(L'.');
    if (dotPos != std::wstring::npos)
    {
        std::wstring ext = m_imageUrls[m_currentDownloadIndex].substr(dotPos);
        if (ext.length() <= 5) // 假设扩展名不超过5个字符
        {
            filename << ext;
            return filename.str();
        }
    }
    
    // 默认使用.jpg
    filename << L".jpg";
    return filename.str();
}


void BrowserWindow::SetupDownloadHandler()
{
    if (m_tabs.find(m_activeTabId) != m_tabs.end() && m_tabs.at(m_activeTabId)->m_contentWebView)
    {
        auto webview10 = m_tabs.at(m_activeTabId)->m_contentWebView.try_query<ICoreWebView2_10>();
        if (!webview10)
        {
            OutputDebugString(L"WebView2 version does not support download API\n");
            return;
        }

        // 移除旧的下载监听器（如果存在）
        if (m_downloadStartingToken.value != 0)
        {
            webview10->remove_DownloadStarting(m_downloadStartingToken);
        }


        // 设置新的下载监听器
        webview10->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                    wil::com_ptr<ICoreWebView2DownloadOperation> download;
                    RETURN_IF_FAILED(args->get_DownloadOperation(&download));
                    
                    wil::unique_cotaskmem_string uri;
                    RETURN_IF_FAILED(download->get_Uri(&uri));

                    // 确保下载目录存在
                    std::wstring downloadsDir = Util::GetCurrentExeDirectory() + L"\\downloads";
                    if (!CreateDirectory(downloadsDir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
                    {
                        OutputDebugString(L"Could not create downloads directory\n");
                        return S_OK;
                    }

                    // 设置下载路径
                    std::wstring filename = GetNextDownloadFilename();
                    std::wstring fullPath = downloadsDir + L"\\" + filename;
                    
                    // 处理下载
                    args->put_ResultFilePath(fullPath.c_str());
                    args->put_Handled(TRUE);
                    
                    // 保存下载操作引用
                    m_downloadOperation = download;
                    
                  

                   // 注册下载进度监听
                    EventRegistrationToken token;
                    HRESULT hr = download->add_BytesReceivedChanged(
                        Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
                            [](ICoreWebView2DownloadOperation* download, IUnknown* args) -> HRESULT {
                                INT64 bytesReceived = 0;
                                download->get_BytesReceived(&bytesReceived);
                                UINT64 unsignedBytesReceived = static_cast<UINT64>(bytesReceived);
            
                                // 打印进度信息（调试用）
                                wil::unique_cotaskmem_string uri;
                                download->get_Uri(&uri);
                                std::wstring debugMsg = L"Download progress: " + std::to_wstring(bytesReceived) + 
                                                      L" bytes received for " + std::wstring(uri.get()) + L"\n";
                                OutputDebugString(debugMsg.c_str());
            
                                return S_OK;
                            }).Get(), &token);

                    if (FAILED(hr)) {
                        OutputDebugString(L"Failed to register BytesReceivedChanged event\n");
                    }

                    // 监听状态变更
                    download->add_StateChanged(
                        Callback<ICoreWebView2StateChangedEventHandler>(
                            [this](ICoreWebView2DownloadOperation* download, IUnknown* args) -> HRESULT {
                                COREWEBVIEW2_DOWNLOAD_STATE state;
                                download->get_State(&state);
                                switch (state) {
                                    case COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS:
                                        break;
                                    case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED:
                                        OutputDebugString(L"Download interrupted\n");
                                        // 下载失败也继续下一个
                                        PostMessage(m_hWnd, WM_APP_DOWNLOAD_NEXT, 0, 0);
                                        break;
                                    case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED:
                                          OutputDebugString(L"Download completed\n");
                                        // 下载完成后继续下一个
                                         PostMessage(m_hWnd, WM_APP_DOWNLOAD_NEXT, 0, 0);
                                        break;
                                }
                                return S_OK;
                            }).Get(), &token);

                    return S_OK;
                }).Get(), &m_downloadStartingToken);
    }
}

void BrowserWindow::SetupDownloaderHandler(const wchar_t* imagePath)
{
    IsInImageDownloadMode = true;
    if (m_tabs.find(m_activeTabId) != m_tabs.end() && m_tabs.at(m_activeTabId)->m_contentWebView)
    {

        auto webview10 = m_tabs.at(m_activeTabId)->m_contentWebView.try_query<ICoreWebView2_10>();
    if (!webview10)
    {
        OutputDebugString(L"WebView2 version does not support download API\n");
        return;
    }

    // 移除旧的下载监听器（如果存在）
    if (m_downloadStartingToken.value != 0)
    {
        webview10->remove_DownloadStarting(m_downloadStartingToken);
    }


    // 设置新的下载监听器
    webview10->add_DownloadStarting(
        Callback<ICoreWebView2DownloadStartingEventHandler>(
            [this, imagePath](ICoreWebView2* sender, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                wil::com_ptr<ICoreWebView2DownloadOperation> download;
                RETURN_IF_FAILED(args->get_DownloadOperation(&download));
                    
                wil::unique_cotaskmem_string uri;
                RETURN_IF_FAILED(download->get_Uri(&uri));

                // 处理下载
                args->put_ResultFilePath(imagePath);
                args->put_Handled(TRUE);
                    
                // 保存下载操作引用
                m_downloadOperation = download;
                    
                  

                // 注册下载进度监听
                EventRegistrationToken token;
                HRESULT hr = download->add_BytesReceivedChanged(
                    Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
                        [](ICoreWebView2DownloadOperation* download, IUnknown* args) -> HRESULT {
                            INT64 bytesReceived = 0;
                            download->get_BytesReceived(&bytesReceived);
                            UINT64 unsignedBytesReceived = static_cast<UINT64>(bytesReceived);
            
                            // 打印进度信息（调试用）
                            wil::unique_cotaskmem_string uri;
                            download->get_Uri(&uri);
                            std::wstring debugMsg = L"Download progress: " + std::to_wstring(bytesReceived) + 
                                                    L" bytes received for " + std::wstring(uri.get()) + L"\n";
                            OutputDebugString(debugMsg.c_str());
            
                            return S_OK;
                        }).Get(), &token);

                if (FAILED(hr)) {
                    OutputDebugString(L"Failed to register BytesReceivedChanged event\n");
                }

                // 监听状态变更
                download->add_StateChanged(
                    Callback<ICoreWebView2StateChangedEventHandler>(
                        [this, imagePath](ICoreWebView2DownloadOperation* download, IUnknown* args) -> HRESULT {
                            COREWEBVIEW2_DOWNLOAD_STATE state;
                            download->get_State(&state);
                            switch (state) {
                                case COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS:
                                    break;
                                case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED:
                                    OutputDebugString(L"Download interrupted\n");
                                    WriteImagePathToSharedMemory(imagePath, true);
                                    break;
                                case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED:
                                    OutputDebugString(L"Download completed\n");
                                    // 下载完成
                                    WriteImagePathToSharedMemory(imagePath, false);
                                    break;
                            }
                            return S_OK;
                        }).Get(), &token);

                return S_OK;
            }).Get(), &m_downloadStartingToken);
    }
}



void BrowserWindow::LoadImageUrlsFromFile()
{
    std::wstring urlsFile;
    std::wifstream file;

    // 1. 优先尝试打开全局 g_urlsFile
    if (!g_urlsFile.empty()) 
    {
        file.open(g_urlsFile);
        if (file.is_open()) 
        {
            urlsFile = g_urlsFile;
            OutputDebugString(L"Successfully opened global urls file\n");
        }
        else
        {
            OutputDebugString(L"Failed to open global urls file, trying local...\n");
        }
    }

    // 2. 如果全局文件未定义或打开失败，尝试本地文件
    if (!file.is_open())
    {
        urlsFile = Util::GetCurrentExeDirectory() + L"\\urls.txt";
        file.open(urlsFile);
    
        if (file.is_open())
        {
            OutputDebugString(L"Successfully opened local urls file\n");
        }
    }

    // 3. 最终检查是否成功打开了任一文件
    if (!file.is_open())
    {
        OutputDebugString(L"Error: Could not open any urls file (global or local)\n");
        return;
    }


    m_imageUrls.clear();
    std::wstring line;
    while (std::getline(file, line))
    {
        if (!line.empty())
        {
            m_imageUrls.push_back(line);
        }
    }
}

void BrowserWindow::DownloadNextImageWithNavigate()
{
    if (m_currentDownloadIndex >= m_imageUrls.size())
    {
        OutputDebugString(L"All downloads completed\n");
        return;
    }

    OutputDebugString(L"Processing download: ");
    OutputDebugString(m_imageUrls[m_currentDownloadIndex].c_str());
    OutputDebugString(L"\n");

    if (m_tabs.find(m_activeTabId) != m_tabs.end() && m_tabs.at(m_activeTabId)->m_contentWebView)
    {
        // 确保下载处理器已设置
        SetupDownloadHandler();
        
        // 导航到URL
        m_tabs.at(m_activeTabId)->m_contentWebView->Navigate(m_imageUrls[m_currentDownloadIndex].c_str());
    }
    else
    {
        OutputDebugString(L"No active tab or webview available, moving to next download\n");
        PostMessage(m_hWnd, WM_APP_DOWNLOAD_NEXT, 0, 0);
    }
}


void BrowserWindow::TriggerDownload(ICoreWebView2* webview) {
  

    // 执行检测脚本
    webview->ExecuteScript(
        LR"JS(
            (function() {
                try {
                    return checkAndDownloadImage();
                } catch(e) {
                    console.error('Image detection failed:', e);
                    return false;
                }
                
                function checkAndDownloadImage() {
                    // 情况1：检查是否是纯图片文档（如直接打开jpg/png）
                    if (document.contentType && document.contentType.startsWith('image/')) {
                        downloadImage(window.location.href);
                        return true;
                    }

                    // 情况2：检查<body>是否只包含单个<img>标签
                    const bodyChildren = document.body.children;
                    if (bodyChildren.length === 1 && bodyChildren[0] instanceof HTMLImageElement) {
                        downloadImage(bodyChildren[0].src);
                        return true;
                    }

                    // 情况3：检查背景图是否是唯一内容
                    const bgImage = window.getComputedStyle(document.body).backgroundImage;
                    if (bgImage && bgImage !== 'none' && document.body.innerText.trim() === '') {
                        const imgUrl = bgImage.replace(/^url$["']?/, '').replace(/["']?$$/, '');
                        downloadImage(imgUrl);
                        return true;
                    }

                    return false;

                    function downloadImage(url) {
                        const link = document.createElement('a');
                        link.href = url;
                        link.download = url.split('/').pop() || 'download';
                        document.body.appendChild(link);
                        link.click();
                        setTimeout(() => document.body.removeChild(link), 100);
                    }
                 }

            })()
        )JS",
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT errorCode, const wchar_t* resultJson) -> HRESULT {
                if (SUCCEEDED(errorCode)) {
                    // 如果返回false表示不是图片页面
                    std::wstring result = Util::ParseJsonBool(resultJson);
                    if (result == L"true") {
                        OutputDebugString(L"Image download triggered\n");
                    }
                }
                return S_OK;
            }).Get());


    //// 方法2：用 JS 强制下载（后备方案）
    //std::wstring js = L"fetch('" + std::wstring(url) + L"')"
    //    L".then(res => res.blob())"
    //    L".then(blob => {"
    //    L"  const a = document.createElement('a');"
    //    L"  a.href = URL.createObjectURL(blob);"
    //    L"  a.download = '" + Util::GetFileNameFromUrl(url) + L"';"
    //    L"  a.click();"
    //    L"})";
    //webview->ExecuteScript(js.c_str(), nullptr);
}


//共享内存相关
// 初始化共享内存和互斥锁
bool BrowserWindow::InitSharedMemory()
{
    // 创建互斥锁
    m_hSharedMemoryMutex = CreateMutexW(nullptr, FALSE, m_sharedMemoryMutexName);
    if (m_hSharedMemoryMutex == nullptr)
    {
        OutputDebugString(L"Failed to create shared memory mutex\n");
        return false;
    }

    // 等待获取互斥锁
    DWORD waitResult = WaitForSingleObject(m_hSharedMemoryMutex, 5000); // 5秒超时
    if (waitResult != WAIT_OBJECT_0)
    {
        OutputDebugString(L"Failed to acquire shared memory mutex\n");
        return false;
    }

    // 创建共享内存
    m_hSharedMemory = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        m_sharedMemorySize,
        m_sharedMemoryName);

    if (m_hSharedMemory == nullptr)
    {
        OutputDebugString(L"Failed to create shared memory\n");
        ReleaseMutex(m_hSharedMemoryMutex);
        return false;
    }

    // 映射共享内存视图
    m_pSharedMemory = MapViewOfFile(
        m_hSharedMemory,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        m_sharedMemorySize);

    if (m_pSharedMemory == nullptr)
    {
        OutputDebugString(L"Failed to map view of shared memory\n");
        CloseHandle(m_hSharedMemory);
        m_hSharedMemory = nullptr;
        ReleaseMutex(m_hSharedMemoryMutex);
        return false;
    }

    // 初始化共享内存结构
    SharedMemoryData* sharedData = static_cast<SharedMemoryData*>(m_pSharedMemory);
    ZeroMemory(sharedData, m_sharedMemorySize);
    sharedData->PID = GetCurrentProcessId(); // 设置当前进程ID

    // 释放互斥锁
    ReleaseMutex(m_hSharedMemoryMutex);

    return true;
}

// 读取共享内存
void BrowserWindow::ReadFromSharedMemory()
{
    if (m_pSharedMemory == nullptr)
        return;

    // 获取互斥锁
    DWORD waitResult = WaitForSingleObject(m_hSharedMemoryMutex, 5000);
    if (waitResult != WAIT_OBJECT_0)
    {
        OutputDebugString(L"Failed to acquire mutex for reading shared memory\n");
        return;
    }

    SharedMemoryData* sharedData = static_cast<SharedMemoryData*>(m_pSharedMemory);

    // 检查是否有新的URL需要处理
    if (sharedData->URLReady && !sharedData->HTMLReady && sharedData->PID != GetCurrentProcessId())
    {
        // 处理URL导航
        if (m_tabs.find(m_activeTabId) != m_tabs.end() && 
            m_tabs.at(m_activeTabId)->m_contentWebView)
        {
            sharedData->URLReady = false;//读出来URL就不要再读了

            if (sharedData->imagePath && sharedData->ImageReady) {
                SetupDownloaderHandler(sharedData->imagePath);
            }

            m_tabs.at(m_activeTabId)->m_contentWebView->Navigate(sharedData->URL);

            std::error_code ec;
            if (std::filesystem::exists(sharedData->URL, ec)) {
                IsInImageDownloadMode = true;
                g_urlsFile = sharedData->URL;
                // 设置定时器，等待WebView完全初始化
                SetTimer(m_hWnd, 2, 1000*5, [](HWND hWnd, UINT, UINT_PTR, DWORD) {
                    KillTimer(hWnd, 2);
                    BrowserWindow* window = reinterpret_cast<BrowserWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                    if (window) {
                        window->StartDownloadProcess();
                    }
                });
            }
        }
    }

 



    // 释放互斥锁
    ReleaseMutex(m_hSharedMemoryMutex);
}

// 写入HTML到共享内存
void BrowserWindow::WriteHtmlToSharedMemory(const std::wstring& html)
{
    if (m_pSharedMemory == nullptr)
        return;

    // 获取互斥锁
    DWORD waitResult = WaitForSingleObject(m_hSharedMemoryMutex, 5000);
    if (waitResult != WAIT_OBJECT_0)
    {
        OutputDebugString(L"Failed to acquire mutex for writing HTML\n");
        return;
    }

    SharedMemoryData* sharedData = static_cast<SharedMemoryData*>(m_pSharedMemory);
    
    // 写入HTML数据
    size_t copySize = min(html.size(), sizeof(sharedData->HTML) / sizeof(wchar_t) - 1);
    wcsncpy_s(sharedData->HTML, html.c_str(), copySize);
    sharedData->HTMLReady = true;
    sharedData->URLReady = false;
    sharedData->PID = GetCurrentProcessId(); // 更新进程ID

    // 释放互斥锁
    ReleaseMutex(m_hSharedMemoryMutex);
}

// 写入Cookies到共享内存
void BrowserWindow::WriteCookiesToSharedMemory(const std::wstring& cookies)
{
    if (m_pSharedMemory == nullptr)
        return;

    // 获取互斥锁
    DWORD waitResult = WaitForSingleObject(m_hSharedMemoryMutex, 5000);
    if (waitResult != WAIT_OBJECT_0)
    {
        OutputDebugString(L"Failed to acquire mutex for writing cookies\n");
        return;
    }

    SharedMemoryData* sharedData = static_cast<SharedMemoryData*>(m_pSharedMemory);
    
    // 写入Cookies数据
    size_t copySize = min(cookies.size(), sizeof(sharedData->cookies) / sizeof(wchar_t) - 1);
    wcsncpy_s(sharedData->cookies, cookies.c_str(), copySize);
    sharedData->CookiesReady = true;
    sharedData->PID = GetCurrentProcessId(); // 更新进程ID

    // 释放互斥锁
    ReleaseMutex(m_hSharedMemoryMutex);
}

// 写入IMAGEPATH到共享内存
void BrowserWindow::WriteImagePathToSharedMemory(const std::wstring& imagePath, bool isReady)
{
    if (m_pSharedMemory == nullptr)
        return;

    // 获取互斥锁
    DWORD waitResult = WaitForSingleObject(m_hSharedMemoryMutex, 5000);
    if (waitResult != WAIT_OBJECT_0)
    {
        OutputDebugString(L"Failed to acquire mutex for writing HTML\n");
        return;
    }

    SharedMemoryData* sharedData = static_cast<SharedMemoryData*>(m_pSharedMemory);
    
    // 写入数据
    size_t copySize = min(imagePath.size(), sizeof(sharedData->imagePath) / sizeof(wchar_t) - 1);
    wcsncpy_s(sharedData->imagePath, imagePath.c_str(), copySize);
    sharedData->ImageReady = isReady;
    sharedData->URLReady = false;
    sharedData->PID = GetCurrentProcessId(); // 更新进程ID

    // 释放互斥锁
    ReleaseMutex(m_hSharedMemoryMutex);
}

// 清理共享内存资源
void BrowserWindow::CleanupSharedMemory()
{
    // 获取互斥锁
    if (m_hSharedMemoryMutex)
    {
        WaitForSingleObject(m_hSharedMemoryMutex, INFINITE);
    }

    // 清理共享内存映射
    if (m_pSharedMemory)
    {
        UnmapViewOfFile(m_pSharedMemory);
        m_pSharedMemory = nullptr;
    }
    
    // 关闭共享内存句柄
    if (m_hSharedMemory)
    {
        CloseHandle(m_hSharedMemory);
        m_hSharedMemory = nullptr;
    }

    // 释放互斥锁并关闭句柄
    if (m_hSharedMemoryMutex)
    {
        ReleaseMutex(m_hSharedMemoryMutex);
        CloseHandle(m_hSharedMemoryMutex);
        m_hSharedMemoryMutex = nullptr;
    }
}