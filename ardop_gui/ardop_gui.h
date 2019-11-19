#ifndef ARDOP_GUI_H
#define ARDOP_GUI_H
#include "QtNetwork/QUdpSocket"
#include "QDialog"
#include <QMainWindow>
#include <QLabel>

namespace Ui {
    class ARDOP_GUI;
}

class ARDOP_GUI : public QMainWindow
{
	Q_OBJECT

public:
	ARDOP_GUI(QWidget *parent = Q_NULLPTR);
    ~ARDOP_GUI();

private slots:
	void readPendingDatagrams();
	void socketError();
	void MyTimerSlot();
	void rxTimerSlot();
	void Configure();
	void setWaterfall();
	void setSpectrum();
	void setDisabled();
	void setSendID();
	void setSendCWID();
	void setSend2ToneTest();

private:
    Ui::ARDOP_GUI *ui;
    QImage *Constellation;
	QImage *Waterfall;
	QImage *RXLevel;
	QUdpSocket * udpSocket;
	QMenu *configMenu;
	QMenu *graphicsMenu;
	QMenu *sendMenu;
	QMenu *abortMenu;
//	QMenu *configMenu;
	QAction *actConfigure;
	QAction *actWaterfall;
	QAction *actSpectrum;
	QAction *actDisabled;
	QAction *actSendID;
	QAction *actTwoToneTest;
	QAction *actSendCWID;
	QDialog *Config;
	QLabel *Busy;
	QTimer *rxtimer;

	void RefreshLevel(unsigned int Level);
	void RefreshConstellation(unsigned char * Data, int Count);
	void RefreshWaterfall(unsigned char * Data, int Count);
	void RefreshSpectrum(unsigned char * Data);
	void SetLEDS(unsigned char * Data);

};

#endif // ARDOP_GUI_H
