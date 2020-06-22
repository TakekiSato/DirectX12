#include <Windows.h>
#include <tchar.h>
#ifdef _DEBUG
#include <iostream>
#endif // !_DEBUG
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define WINDOW_CLASS_NAME TEXT("DirectX12Sample")

using namespace std;
using namespace DirectX;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
ID3D12DescriptorHeap* rtvHeaps = nullptr;

// @brief コンソール画面にフォーマット付き文字列を表示
// @param formatフォーマット（%dとか%fとかの）
// @param 可変長引数
// @remarks この関数はデバッグ用です。デバッグ時にしか動作しません
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
    va_list valist;
    va_start(valist, format);
    printf(format, valist);
    va_end(valist);
#endif // _DEBUG

}

// 面倒だけど書かなければいけない関数
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // ウィンドウが破棄されたら呼ばれる
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);  // OSに対して「もうこのアプリは終わる」と伝える
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
    // 既定の処理を行う
}

// デバッグレイヤーを有効化する
void EnableDebugLayer() {
    ID3D12Debug* debugLayer = nullptr;
    auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
    debugLayer->EnableDebugLayer();  // デバッグレイヤ―を有効化する
    debugLayer->Release();  // 有効化したらインターフェースを開放する
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
    /*DebugOutputFormatString("Show window test.");
    getchar();
    return 0;*/
    // ウィンドウクラスの生成＆登録
    WNDCLASSEX w = {};

    w.cbSize = sizeof(WNDCLASSEX);
    w.lpfnWndProc = (WNDPROC)WindowProcedure;  // コールバック関数の指定
    w.lpszClassName = WINDOW_CLASS_NAME;  // アプリケーションクラス名（適当でよい）
    w.hInstance = GetModuleHandle(nullptr);  // ハンドルの取得

    RegisterClassEx(&w);  // アプリケーションクラス（ウィンドウの指定をOSに伝える）

    RECT wrc = { 0, 0, window_width, window_height };  // ウィンドウサイズを決める

    // 関数を使ってウィンドウのサイズを補正する
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウオブジェクトの生成
    HWND hwnd = CreateWindow(w.lpszClassName,  // クラス名指定
        WINDOW_CLASS_NAME,  // タイトルバーの文字
        WS_OVERLAPPEDWINDOW, // タイトルバーと境界線があるウィンドウ
        CW_USEDEFAULT,   // 表示x座標はOSにお任せ
        CW_USEDEFAULT,   // 表示y座標はOSにお任せ
        wrc.right - wrc.left,  // ウィンドウ幅
        wrc.bottom - wrc.top,  // ウィンドウ高
        nullptr,               // 親ウィンドウハンドル
        nullptr,               // メニューハンドル
        w.hInstance,           // 呼び出しアプリケーションハンドル
        nullptr);

#ifdef _DEBUG
    auto result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
    auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif // _DEBUG

    // アダプターの列挙用
    std::vector <IDXGIAdapter*> adapters;
    // ここに特定の名前を持つアダプターオブジェクトが入る
    IDXGIAdapter* tmpAdapter = nullptr;
    for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        adapters.push_back(tmpAdapter);
    }
    // アダプターの検索(どのグラフィックボードを指定するか)
    for (auto adpt : adapters) {
        DXGI_ADAPTER_DESC adesc = {};
        adpt->GetDesc(&adesc);  // アダプターの説明オブジェクト取得
        std::wstring strDesc = adesc.Description;
        // 探したいアダプターの名前を確認
        if (strDesc.find(L"NVIDIA") != std::string::npos) {
            tmpAdapter = adpt;
            break;
        }
    }

#ifdef _DEBUG
    // デバッグレイヤ―をオンに
    EnableDebugLayer();
#endif // _DEBUG


    // フィーチャーレベル列挙
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL featureLevel;
    // Direct3Dデバイスの初期化
    for (auto lv : levels) {
        if (D3D12CreateDevice(nullptr, lv, IID_PPV_ARGS(&_dev)) == S_OK) {
            featureLevel = lv;
            break;  // 生成可能なバージョンが見つかったらループを打ち切り
        }
    }

    result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
    result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    
    // タイムアウトなし
    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    // アダプターを１つしか使わないときは0でよい
    cmdQueueDesc.NodeMask = 0;
    // プライオリティは特に指定なし
    cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    // コマンドリストと合わせる
    cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    // キュー生成
    result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

    // スワップチェーン生成
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = window_width;
    swapchainDesc.Height = window_height;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.Stereo = false;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;  // バックバッファーは伸び縮み可能
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // フリップ後は速やかに破棄
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;  // 特に指定なし
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;  // ウィンドウ⇔フルスクリーン切り替え可能
    result = _dxgiFactory->CreateSwapChainForHwnd(
        _cmdQueue, 
        hwnd, 
        &swapchainDesc, 
        nullptr, 
        nullptr, 
        (IDXGISwapChain1**)&_swapchain);

    // ディスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;  // レンダーターゲットビューなのでRTV
    heapDesc.NodeMask = 0;  // 複数のGPUがある場合に識別するためのビットフラグ。今回はGPUを１つだけ使用する想定なので0
    heapDesc.NumDescriptors = 2;  // バッファー表裏の2つ
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // 特に指定なし(ビューに当たる情報をシェーダー側から参照する必要があるか同化を指定する列挙子)
    result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

    // スワップチェーンのメモリと紐づけ
    std::vector<ID3D12Resource*> _backBuffers(swapchainDesc.BufferCount);
    for (int idx = 0; idx < swapchainDesc.BufferCount; ++idx) {
        result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        _dev->CreateRenderTargetView(_backBuffers[idx], nullptr, handle);
    }

    // フェンスの作成
    ID3D12Fence* _fence = nullptr;
    UINT64 _fenceVal = 0;
    result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

    // ウィンドウ表示
    ShowWindow(hwnd, SW_SHOW);

    XMFLOAT3 vertices[] = {
    {-1.0f, -1.0f, 0.0f}, // 左下
    {-1.0f, 1.0f, 0.0f}, // 左上
    {1.0f, -1.0f, 0.0f}, // 右下
    };

    D3D12_HEAP_PROPERTIES heapprop = {};
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc = {};
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = sizeof(vertices);  // 頂点情報が入るだけのサイズ
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.SampleDesc.Count = 1;
    resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* vertBuff = nullptr;
    result = _dev->CreateCommittedResource(
        &heapprop,
        D3D12_HEAP_FLAG_NONE,
        &resdesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertBuff)
    );

    XMFLOAT3* vertMap = nullptr;
    result = vertBuff->Map(0, nullptr, (void**)&vertMap);
    std::copy(std::begin(vertices), std::end(vertices), vertMap);
    vertBuff->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();  // バッファーの仮想アドレス
    vbView.SizeInBytes = sizeof(vertices);  // 全バイト数
    vbView.StrideInBytes = sizeof(vertices[0]);  // 1頂点あたりのバイト数

    ID3DBlob* _vsBlob = nullptr;
    ID3DBlob* _psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    // 頂点シェーダーの読み込み
    result = D3DCompileFromFile(
        L"BasicVertexShader.hlsl",  // シェーダー名
        nullptr,  // defineはなし
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
        "BasicVS", "vs_5_0",  // 関数はBasicVS、対象シェーダーはvs_5_0
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,  // デバッグ用及び最適化なし
        0,
        &_vsBlob, &errorBlob  // エラー時はerrorBlobにメッセージが入る
    );
    if (FAILED(result)) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("ファイルが見当たりません");
        }
        else {
            std::string errstr;
            errstr.resize(errorBlob->GetBufferSize());
            std::copy_n((char*)errorBlob->GetBufferPointer(),
                errorBlob->GetBufferSize(),
                errstr.begin());
            errstr += "\n";
            ::OutputDebugStringA(errstr.c_str());
        }
        exit(1);  // 強制終了？
    }

    // ピクセルシェーダーの読み込み
    result = D3DCompileFromFile(
        L"BasicPixelShader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "BasicPS", "ps_5_0", // 関数はBasicPS、対象シェーダーはps_5_0
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &_psBlob, &errorBlob
    );
    if (FAILED(result)) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("ファイルが見当たりません");
        }
        else {
            std::string errstr;
            errstr.resize(errorBlob->GetBufferSize());
            std::copy_n((char*)errorBlob->GetBufferPointer(),
                errorBlob->GetBufferSize(),
                errstr.begin());
            errstr += "\n";
            ::OutputDebugStringA(errstr.c_str());
        }
        exit(1);  // 強制終了？
    }

    // 頂点レイアウトの定義
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // グラフィックスパイプラインの設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
    gpipeline.pRootSignature = nullptr;  // 後ほど設定
    // シェーダーのセット(頂点シェーダー＆ピクセルシェーダー)
    gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
    gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
    gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
    gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();
    // デフォルトのサンプルマスクを表す定数(0xffffffff)
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    // まだアンチエイリアスは使わないためfalse
    gpipeline.RasterizerState.MultisampleEnable = false;
    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // カリングしない
    gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;  // 中身を塗りつぶす
    gpipeline.RasterizerState.DepthClipEnable = true;  // 震度方向のクリッピングは有効に
    // ブレンドステート設定
    gpipeline.BlendState.AlphaToCoverageEnable = false;
    gpipeline.BlendState.IndependentBlendEnable = false;
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = false;
    renderTargetBlendDesc.LogicOpEnable = false;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;
    // 入力レイアウト設定
    gpipeline.InputLayout.pInputElementDescs = inputLayout;  // レイアウト先頭アドレス
    gpipeline.InputLayout.NumElements = _countof(inputLayout);  // レイアウト配列の要素数
    gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;  // カットなし
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;  // 三角形で構成
    // レンダーターゲットの設定
    gpipeline.NumRenderTargets = 1; // 今は１つのみ
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0~1に正規化されたRGBA
    // アンチエイリアシングのためのサンプル数設定
    gpipeline.SampleDesc.Count = 1; // サンプリングは1ピクセルにつき1
    gpipeline.SampleDesc.Quality = 0; // クオリティは最低設定
    

    // ルートシグネチャの作成
    ID3D12RootSignature* rootsignature = nullptr;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // 頂点情報(入力アセンブラ)がある
    ID3DBlob* rootSigBlob = nullptr;
    result = D3D12SerializeRootSignature(
        &rootSignatureDesc, // ルートシグネチャ設定
        D3D_ROOT_SIGNATURE_VERSION_1_0, // ルートシグネチャバージョン
        &rootSigBlob,
        &errorBlob);
    result = _dev->CreateRootSignature(
        0, // nodemask。0でよい
        rootSigBlob->GetBufferPointer(), // シェーダーの時と同様
        rootSigBlob->GetBufferSize(), // シェーダーの時と同様
        IID_PPV_ARGS(&rootsignature));
    gpipeline.pRootSignature = rootsignature;

    // グラフィックスパイプラインステートオブジェクトの生成
    ID3D12PipelineState* _pipelinestate = nullptr;
    result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

    MSG msg{};
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // アプリケーションが終わるときにmessageがWM_QUITになる
        if (msg.message == WM_QUIT)
        {
            break;
        }

        // DirectX処理
        // バックバッファのインデックスを取得
        auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

        // リソースバリアの設定
        D3D12_RESOURCE_BARRIER BarrierDesc = {};
        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;  // 遷移
        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;  // 特に指定なし
        BarrierDesc.Transition.pResource = _backBuffers[bbIdx];  // バックバッファリソース
        BarrierDesc.Transition.Subresource = 0;
        BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;  // 直前はPRESENT状態
        BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;  // 今からレンダーターゲット状態
        _cmdList->ResourceBarrier(1, &BarrierDesc);

        // レンダーターゲットを指定
        auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        _cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

        // 画面クリア
        float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };  // 黄色
        _cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

        // レンダーターゲットからPresent状態に移行
        BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;  // 今からレンダーターゲット状態
        BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;  // 直前はPRESENT状態
        _cmdList->ResourceBarrier(1, &BarrierDesc);

        // 命令のクローズ
        _cmdList->Close();
        // コマンドリストの実行
        ID3D12CommandList* cmdlists[] = { _cmdList };
        _cmdQueue->ExecuteCommandLists(1, cmdlists);
        ///待ち
        _cmdQueue->Signal(_fence, ++_fenceVal);
        if (_fence->GetCompletedValue() != _fenceVal) {
            // イベントハンドルの取得
            auto event = CreateEvent(nullptr, false, false, nullptr);
            _fence->SetEventOnCompletion(_fenceVal, event);
            // イベントが発生するまで待ち続ける(INFINITE)
            WaitForSingleObject(event, INFINITE);
            // イベントハンドルを閉じる
            CloseHandle(event);
        }
        _cmdAllocator->Reset();  // キューをクリア
        _cmdList->Reset(_cmdAllocator, nullptr);  // 再びコマンドリストをためる準備

        // フリップ
        _swapchain->Present(1, 0);
    }

    // もうクラスは使わないので登録解除する
    UnregisterClass(w.lpszClassName, w.hInstance);
}
