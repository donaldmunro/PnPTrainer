#ifndef _MAIN_HH
#define _MAIN_HH

#include <optional>

#include <QWidget>

void msg(const char *psz);
void msg(const std::stringstream &errs);
bool yesno(QWidget* parent, const char *title, const char *msg);


void stop();
//bool open_dir(const std::string& dir, std::string& imgfile, std::string& plyfile);
#endif
