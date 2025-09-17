#include "Application.h"

int main()
{
    Application app;
    if (app.createWindow() != 0)
    {
        return -1;
    }

    app.run();

    return 0;
}