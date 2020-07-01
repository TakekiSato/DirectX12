// ポリゴン表示
#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>

#include <d3dcompiler.h>
#ifdef _DEBUG
#include <iostream>
#endif // !_DEBUG

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;


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
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);  // OSに対して「もうこのアプリは終わる」と伝える
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理を行う
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12Device* _dev = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

// デバッグレイヤーを有効化する
void EnableDebugLayer() {
    ID3D12Debug* debugLayer = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
        debugLayer->EnableDebugLayer();  // デバッグレイヤ―を有効化する
        debugLayer->Release();  // 有効化したらインターフェースを開放する
    }
}

#ifdef _DEBUG
int main() {
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
    DebugOutputFormatString("Show window test.");
    HINSTANCE hInst = GetModuleHandle(nullptr);
    // ウィンドウクラスの生成＆登録
    WNDCLASSEX w = {};
    w.cbSize = sizeof(WNDCLASSEX);
    w.lpfnWndProc = (WNDPROC)WindowProcedure;  // コールバック関数の指定
    w.lpszClassName = _T("DirectXTest");  // アプリケーションクラス名（適当でよい）
    w.hInstance = GetModuleHandle(0);  // ハンドルの取得
    RegisterClassEx(&w);  // アプリケーションクラス（ウィンドウの指定をOSに伝える）

    RECT wrc = { 0, 0, window_width, window_height };  // ウィンドウサイズを決める
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // 関数を使ってウィンドウのサイズを補正する

    // ウィンドウオブジェクトの生成
    HWND hwnd = CreateWindow(w.lpszClassName,  // クラス名指定
        _T("DX12 単純ポリゴンテスト"),  // タイトルバーの文字
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
    // デバッグレイヤ―をオンに
    EnableDebugLayer();
#endif // _DEBUG
    // DirectX12まわり初期化
    // フィーチャーレベル列挙
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
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

    // Direct3Dデバイスの初期化
    D3D_FEATURE_LEVEL featureLevel;
    for (auto lv : levels) {
        if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK) {
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
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // 特に指定なし(ビューに当たる情報をシェーダー側から参照する必要があるかどうかを指定する列挙子)
    ID3D12DescriptorHeap* rtvHeaps = nullptr;
    result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

    DXGI_SWAP_CHAIN_DESC swcDesc = {};
    result = _swapchain->GetDesc(&swcDesc);
    // スワップチェーンのメモリと紐づけ
    std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
    for (int idx = 0; idx < swcDesc.BufferCount; ++idx) {
        result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
        _dev->CreateRenderTargetView(_backBuffers[idx], nullptr, handle);
        handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // フェンスの作成
    ID3D12Fence* _fence = nullptr;
    UINT64 _fenceVal = 0;
    result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

    // ウィンドウ表示
    ShowWindow(hwnd, SW_SHOW);

    // 頂点データ構造体
    struct Vertex {
        XMFLOAT3 pos; // xyz座標
        XMFLOAT2 uv;  // uv座標
    };

    // 四角形
    Vertex vertices[] = {
        {{-0.4f,-0.7f,0.0f}, {0.0f,1.0f}} ,//左下
        {{-0.4f,0.7f,0.0f}, {0.0f,0.0f}} ,//左上
        {{0.4f,-0.7f,0.0f}, {1.0f,1.0f}} ,//右下
        {{0.4f,0.7f,0.0f}, {1.0f, 0.0f}} ,//右上
    };


    D3D12_HEAP_PROPERTIES heapprop = {};
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resdesc = {};
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = sizeof(vertices);
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.Format = DXGI_FORMAT_UNKNOWN;
    resdesc.SampleDesc.Count = 1;
    resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    //UPLOAD(確保は可能)
    ID3D12Resource* vertBuff = nullptr;
    result = _dev->CreateCommittedResource(
        &heapprop,
        D3D12_HEAP_FLAG_NONE,
        &resdesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertBuff));

    Vertex* vertMap = nullptr;
    result = vertBuff->Map(0, nullptr, (void**)&vertMap);

    std::copy(std::begin(vertices), std::end(vertices), vertMap);

    vertBuff->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();//バッファの仮想アドレス
    vbView.SizeInBytes = sizeof(vertices);//全バイト数
    vbView.StrideInBytes = sizeof(vertices[0]);//1頂点あたりのバイト数

    unsigned short indices[] = { 0,1,2, 2,1,3 };

    ID3D12Resource* idxBuff = nullptr;
    //設定は、バッファのサイズ以外頂点バッファの設定を使いまわして
    //OKだと思います。
    resdesc.Width = sizeof(indices);
    result = _dev->CreateCommittedResource(
        &heapprop,
        D3D12_HEAP_FLAG_NONE,
        &resdesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&idxBuff));

    //作ったバッファにインデックスデータをコピー
    unsigned short* mappedIdx = nullptr;
    idxBuff->Map(0, nullptr, (void**)&mappedIdx);
    std::copy(std::begin(indices), std::end(indices), mappedIdx);
    idxBuff->Unmap(0, nullptr);

    //インデックスバッファビューを作成
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
    ibView.Format = DXGI_FORMAT_R16_UINT;
    ibView.SizeInBytes = sizeof(indices);

    ID3DBlob* _vsBlob = nullptr;
    ID3DBlob* _psBlob = nullptr;

    ID3DBlob* errorBlob = nullptr;
    result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "BasicVS", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &_vsBlob, &errorBlob);
    if (FAILED(result)) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("ファイルが見当たりません");
        }
        else {
            std::string errstr;
            errstr.resize(errorBlob->GetBufferSize());
            std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
            errstr += "\n";
            OutputDebugStringA(errstr.c_str());
        }
        exit(1);//行儀悪いかな…
    }

    // ピクセルシェーダーの読み込み
    result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "BasicPS", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &_psBlob, &errorBlob);
    if (FAILED(result)) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("ファイルが見当たりません");
        }
        else {
            std::string errstr;
            errstr.resize(errorBlob->GetBufferSize());
            std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
            errstr += "\n";
            OutputDebugStringA(errstr.c_str());
        }
        exit(1);//行儀悪いかな…
    }

    // 頂点レイアウトの定義
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { // 座標情報 
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 
            D3D12_APPEND_ALIGNED_ELEMENT, 
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 
        },
        { // uv(追加)
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
            0, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
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

    // ブレンドステート設定
    gpipeline.BlendState.AlphaToCoverageEnable = false;
    gpipeline.BlendState.IndependentBlendEnable = false;


    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

    renderTargetBlendDesc.BlendEnable = false;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    renderTargetBlendDesc.LogicOpEnable = false;
    gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

    // まだアンチエイリアスは使わないためfalse
    gpipeline.RasterizerState.MultisampleEnable = false;
    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // カリングしない
    gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;  // 中身を塗りつぶす
    gpipeline.RasterizerState.DepthClipEnable = true;  // 震度方向のクリッピングは有効に

    //残り
    gpipeline.RasterizerState.FrontCounterClockwise = false;
    gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    gpipeline.RasterizerState.AntialiasedLineEnable = false;
    gpipeline.RasterizerState.ForcedSampleCount = 0;
    gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    gpipeline.DepthStencilState.DepthEnable = false;
    gpipeline.DepthStencilState.StencilEnable = false;

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

    // DescriptorTableメンバの設定
    // DescriptorTableの設定にディスクリプタレンジのアドレスが必要
    // ディスクリプタレンジを設定
    D3D12_DESCRIPTOR_RANGE descTblRange = {};
    descTblRange.NumDescriptors = 1; // テクスチャ1つ
    descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // 種別はテクスチャ
    descTblRange.BaseShaderRegister = 0; // 0番スロットから
    descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // サンプラーの設定
    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 横方向の折り返し
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 縦方向の折り返し
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 奥行きの折り返し
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // ボーダーは黒
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 線形補間
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX; // ミップマップ最大値
    samplerDesc.MinLOD = 0.0f; // ミップマップ最小値
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーから見える
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // リサンプリングしない

    // ルートシグネチャに追加するルートパラメーターの定義
    D3D12_ROOT_PARAMETER rootparam = {};
    rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    // ピクセルシェーダーから見える(利用可能)設定
    rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    // ディスクリプタレンジのアドレス
    rootparam.DescriptorTable.pDescriptorRanges = &descTblRange;
    // ディスクリプタレンジ数
    rootparam.DescriptorTable.NumDescriptorRanges = 1;
    // ルートパラメーターが作成できたのでルートシグネチャにルートパラメーターを設定
    rootSignatureDesc.pParameters = &rootparam; // ルートパラメーターの先頭アドレス
    rootSignatureDesc.NumParameters = 1; // ルートパラメーター数
    // サンプラーをルートシグネチャに設定
    rootSignatureDesc.pStaticSamplers = &samplerDesc;
    rootSignatureDesc.NumStaticSamplers = 1;

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
    rootSigBlob->Release();

    gpipeline.pRootSignature = rootsignature;
    // グラフィックスパイプラインステートオブジェクトの生成
    ID3D12PipelineState* _pipelinestate = nullptr;
    result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

    // ビューポートの作成
    D3D12_VIEWPORT viewport = {};
    viewport.Width = window_width; // 出力先の幅(ピクセル数)
    viewport.Height = window_height; // 出力先の高さ(ピクセル数)
    viewport.TopLeftX = 0; // 出力先の左上座標X
    viewport.TopLeftY = 0; // 出力先の左上座標Y
    viewport.MaxDepth = 1.0f; // 深度最大値
    viewport.MinDepth = 0.0f; // 深度最小値

    // シザー矩形の設定
    D3D12_RECT scissorrect = {};
    scissorrect.top = 0; // 切り抜き上座標
    scissorrect.left = 0; // 切り抜き左座標
    scissorrect.right = scissorrect.left + window_width; // 切り抜き右座標
    scissorrect.bottom = scissorrect.top + window_height; // 切り抜き左座標

    // 仮のノイズテクスチャの作成
    struct TexRGBA {
        unsigned char R, G, B, A;
    };
    std::vector<TexRGBA> texturedata(256 * 256);
    for (auto& rgba : texturedata) {
        rgba.R = rand() % 256;
        rgba.G = rand() % 256;
        rgba.B = rand() % 256;
        rgba.A = 255; // αは1.0とする
    }
    // テクスチャバッファーの作成
    // WriteToSubresourceで転送するためのヒープ設定
    D3D12_HEAP_PROPERTIES texHeapProp = {};
    // 特殊な設定なのでDEFAULTでもUPLOADでもない
    texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    // ライトバック
    texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    // 転送はL0、つまりCPU側から直接行う
    texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    // 単一アダプターのため0
    texHeapProp.CreationNodeMask = 0;
    texHeapProp.VisibleNodeMask = 0;

    // リソースの設定
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBAフォーマット
    resDesc.Width = 256; // 幅
    resDesc.Height = 256; // 高さ
    resDesc.DepthOrArraySize = 1; // 2Dで配列でもないので1
    resDesc.SampleDesc.Count = 1; // 通常テクスチャなのでアンチエイリアシングしない
    resDesc.SampleDesc.Quality = 0; // クオリティは最低
    resDesc.MipLevels = 1; // ミップマップしないのでミップ数は１つ
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2Dテクスチャ用
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // レイアウトは決定しない
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // 特にフラグなし

    // リソースの生成
    ID3D12Resource* texbuff = nullptr;
    result = _dev->CreateCommittedResource(&texHeapProp,
        D3D12_HEAP_FLAG_NONE, // 特に指定なし
        &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // テクスチャ用設定
        nullptr,
        IID_PPV_ARGS(&texbuff));

    // ID3D12Resource::WriteToSubresource()メソッドを使ったテクスチャバッファーの転送
    result = texbuff->WriteToSubresource(
        0,
        nullptr, // 全領域へコピー
        texturedata.data(), //元データアドレス
        sizeof(TexRGBA) * 256, // 1ラインサイズ
        sizeof(TexRGBA) * texturedata.size() // 全サイズ
    );

    // シェーダーリソース用のディスクリプタヒープを作る
    ID3D12DescriptorHeap* texDescHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
    // シェーダーから見えるように
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    // マスクは0
    descHeapDesc.NodeMask = 0;
    // ビューは今のところ1つだけ
    descHeapDesc.NumDescriptors = 1;
    // シェーダーリソースビュー用
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    // 生成
    result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&texDescHeap));

    // シェーダーリソースビューを作る
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA(0.0f~1.0fに正規化)
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc.Texture2D.MipLevels = 1; // ミップマップは使用しないので1

    _dev->CreateShaderResourceView(
        texbuff, // ビューと関連付けるバッファー
        &srvDesc, // テクスチャ設定情報
        texDescHeap->GetCPUDescriptorHandleForHeapStart() // ヒープのどこに割り当てるか
    );

    MSG msg{};
    unsigned int frame = 0;
    while (true) {

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // アプリケーションが終わるときにmessageがWM_QUITになる
        if (msg.message == WM_QUIT) {
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
        BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;  // 直前はPRESENT状態
        BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;  // 今からレンダーターゲット状態

        _cmdList->ResourceBarrier(1, &BarrierDesc);

        // パイプラインステートのセット
        _cmdList->SetPipelineState(_pipelinestate);

        // レンダーターゲットを指定
        auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        _cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);


        // 画面クリア
        float r, g, b;
        r = (float)(0xff & frame >> 16) / 255.0f;
        g = (float)(0xff & frame >> 8) / 255.0f;
        b = (float)(0xff & frame >> 0) / 255.0f;
        float clearColor[] = { r,g,b,1.0f };//黄色
        _cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
        ++frame;
        _cmdList->RSSetViewports(1, &viewport);
        _cmdList->RSSetScissorRects(1, &scissorrect);
        // ルートシグネチャ設定
        _cmdList->SetGraphicsRootSignature(rootsignature);
        // プリミティブトポロジの設定
        _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // 頂点バッファーのセット
        _cmdList->IASetVertexBuffers(0, 1, &vbView);
        // インデックスバッファーのセット
        _cmdList->IASetIndexBuffer(&ibView);

        _cmdList->SetGraphicsRootSignature(rootsignature);
        _cmdList->SetDescriptorHeaps(1, &texDescHeap);
        _cmdList->SetGraphicsRootDescriptorTable(
            0, // ルートパラメーターインデックス
            texDescHeap->GetGPUDescriptorHandleForHeapStart()); // ヒープアドレス

        // 描画命令のセット
        // _cmdList->DrawInstanced(3, 1, 0, 0);
        _cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);


        // レンダーターゲットからPresent状態に移行
        BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;  // 今からレンダーターゲット状態
        BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;  // 直前はPRESENT状態
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
        _cmdList->Reset(_cmdAllocator, _pipelinestate);  // 再びコマンドリストをためる準備

        // フリップ
        _swapchain->Present(1, 0);
    }

    // もうクラスは使わないので登録解除する
    UnregisterClass(w.lpszClassName, w.hInstance);
    return 0;
}
