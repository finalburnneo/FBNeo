#ifndef MAIN_H
#define MAIN_H

void SetNVRAMPath(const char *path);

int MainInit(const char *, const char *);
int MainFrame();
int MainEnd();

int OneTimeInit();
int OneTimeEnd();

#endif
