#include "ardop_gui.h"
#include <QApplication>

extern char Host[];
extern char Port[];

int main(int argc, char *argv[])
{
	if (argc > 2)
	{
		strcpy(Host, argv[1]);
		strcpy(Port, argv[2]);
	}

	QApplication a(argc, argv);
	ARDOP_GUI w;
	w.show();

	return a.exec();
}
