#include <medusa/medusa_app.hpp>
#include <windows.h>
#include <objbase.h>

int main(int argc, char* argv[]) {
    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    medusa::MedusaApp app;
    int result = app.run(argc, argv);

    if (SUCCEEDED(hr_com)) {
        CoUninitialize();
    }
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
