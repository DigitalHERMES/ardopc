#include "ardop_gui.h"
#include "ui_ardop_gui.h"
#include "TabDialog.h"

#include <QtNetwork/QUdpSocket>
#include "QTimer"
#include "QSettings"

int Keepalive = 0;
int GUIActive = 0;			// Set when messages being received

QRgb white = qRgb(255, 255, 255);
QRgb black = qRgb(0, 0, 0);
QRgb green = qRgb(0, 255, 0);
QRgb yellow = qRgb(255, 255, 0);
QRgb cyan = qRgb(0, 255, 255);

// Indexed colour list from ARDOPC

#define WHITE 0
#define Tomato 1
#define Gold 2
#define Lime 3
#define Yellow 4
#define Orange 5
#define Khaki 6
#define Cyan 7
#define DeepSkyBlue 8
#define RoyalBlue 9
#define Navy 10
#define Black 11
#define Goldenrod 12
#define Fuchsia 13

QRgb vbColours[16] = { qRgb(255, 255, 255), qRgb(255, 99, 71), qRgb(255, 215, 0), qRgb(0, 255, 0),
						qRgb(255, 255, 0), qRgb(255, 165, 0), qRgb(240, 240, 140), qRgb(0, 255, 255),
						qRgb(0, 191, 255), qRgb(65, 105, 225), qRgb(0, 0, 128), qRgb(0, 0, 0),
						qRgb(218, 165, 32), qRgb(255, 0, 255) };

unsigned char  WaterfallLines[64][220] = {0};
int NextWaterfallLine = 0;

unsigned int LastLevel = 255;
unsigned int LastBusy = 255;

char Host[256]= "";
char Port[16] = "";
int PortNum = 0;

ARDOP_GUI::ARDOP_GUI(QWidget *parent) : QMainWindow(parent), ui(new Ui::ARDOP_GUI)
{
    char Msg[80];
    QByteArray qb;
    char * ptr;
	int i;

    ui->setupUi(this);

    // Get Host and Port unless provided on command 

	if (Host[0] == 0 && Port[0] == 0)
	{
		QSettings settings("G8BPQ", "ARDOP_GUI");

		qb = settings.value("Host").toByteArray();
		ptr = qb.data();
		strcpy(Host, ptr);

		qb = settings.value("Port").toByteArray();
		ptr = qb.data();
		strcpy(Port, ptr);
	}

    // Create Menus

	configMenu = ui->menuBar->addMenu(tr("File"));
    actConfigure = new QAction(tr("Configure"), this);
    configMenu->addAction(actConfigure);

	graphicsMenu = ui->menuBar->addMenu(tr("Graphics"));
	actWaterfall = new QAction(tr("Waterfall"), this);
	graphicsMenu->addAction(actWaterfall);
	actSpectrum = new QAction(tr("Spectrum"), this);
	graphicsMenu->addAction(actSpectrum);
	actDisabled = new QAction(tr("Disable"), this);
	graphicsMenu->addAction(actDisabled);

	sendMenu = ui->menuBar->addMenu(tr("Send"));
	actSendID = new QAction(tr("Send ID"), this);
	sendMenu->addAction(actSendID);
	actTwoToneTest = new QAction(tr("Send Two Tone Test"), this);
	sendMenu->addAction(actTwoToneTest);
	actSendCWID = new QAction(tr("Send CWID"), this);
	sendMenu->addAction(actSendCWID);

	abortMenu = ui->menuBar->addMenu(tr("Abort"));

	Busy = new QLabel(this);
	Busy->setFixedHeight(20);
	Busy->setAlignment(Qt::AlignCenter);
	Busy->setText("  Channel Busy  ");
	Busy->setStyleSheet("QLabel { background-color : rgb(255, 215, 0); color : black; }");

	ui->menuBar->setCornerWidget(Busy);

	Busy->setVisible(false);

	connect(actConfigure, &QAction::triggered, this, &ARDOP_GUI::Configure);
	connect(actWaterfall, &QAction::triggered, this, &ARDOP_GUI::setWaterfall);
	connect(actSpectrum, &QAction::triggered, this, &ARDOP_GUI::setSpectrum);
	connect(actDisabled, &QAction::triggered, this, &ARDOP_GUI::setDisabled);
	connect(actSendID, &QAction::triggered, this, &ARDOP_GUI::setSendID);
	connect(actSendCWID, &QAction::triggered, this, &ARDOP_GUI::setSendCWID);
	connect(actTwoToneTest, &QAction::triggered, this, &ARDOP_GUI::setSend2ToneTest);

    while (Host[0] == 0 || Port[0] == 0)
    {
        // Request Config

       TabDialog tabdialog(0);
       tabdialog.exec();
    }

    PortNum = atoi(Port);

    udpSocket = new QUdpSocket();
    udpSocket->bind(QHostAddress("0.0.0.0"), PortNum + 1);		// We send from Port + 1

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

    udpSocket->writeDatagram("ARDOP_GUI Running", 17, QHostAddress(Host), PortNum);

    Constellation = new QImage(91, 91, QImage::Format_RGB32);
    Waterfall = new QImage(205, 64, QImage::Format_Indexed8);
    RXLevel = new QImage(150, 10, QImage::Format_RGB32);

    Waterfall->setColorCount(16);

	for (i = 0; i < 16; i++)
	{
		Waterfall->setColor(i, vbColours[i]);
	}

    Constellation->fill(black);

    RefreshLevel(0);

    ui->Constellation->setPixmap(QPixmap::fromImage(*Constellation));
    ui->Waterfall->setPixmap(QPixmap::fromImage(*Waterfall));
    ui->RXLevel->setPixmap(QPixmap::fromImage(*RXLevel));

	ui->PTT->setStyleSheet("QLabel { background-color : rgb(255, 0, 0); color : black; }");
	ui->ISS->setStyleSheet("QLabel { background-color : rgb(0, 255, 0); color : black; }");
	ui->IRS->setStyleSheet("QLabel { background-color : rgb(0, 255, 0); color : black; }");
	ui->TRAFFIC->setStyleSheet("QLabel { background-color : rgb(255, 255, 0); color : black; }");
	ui->ACK->setStyleSheet("QLabel { background-color : rgb(0, 255, 0); color : black; }");
	ui->PTT->setVisible(false);
	ui->ISS->setVisible(false);
	ui->IRS->setVisible(false);
	ui->TRAFFIC->setVisible(false);
	ui->ACK->setVisible(false);

	sprintf(Msg, "ARDOP_GUI %s:%s Waiting...", Host, Port);
	this->setWindowTitle(Msg);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(MyTimerSlot()));
    timer->start(1000);

	rxtimer = new QTimer(this);
	connect(rxtimer, SIGNAL(timeout()), this, SLOT(rxTimerSlot()));

//	QCoreApplication::sendPostedEvents();
}

void ARDOP_GUI::Configure()
{
	TabDialog tabdialog(0);
	tabdialog.exec();
}

void ARDOP_GUI::setWaterfall()
{
	udpSocket->writeDatagram("Waterfall", 9, QHostAddress(Host), PortNum);
}

void ARDOP_GUI::setSpectrum()
{
	udpSocket->writeDatagram("Spectrum", 8, QHostAddress(Host), PortNum);
}

void ARDOP_GUI::setDisabled()
{
	udpSocket->writeDatagram("Disable", 7, QHostAddress(Host), PortNum);
}

void ARDOP_GUI::setSendID()
{
	udpSocket->writeDatagram("SENDID", 6, QHostAddress(Host), PortNum);
}

void ARDOP_GUI::setSendCWID()
{
	udpSocket->writeDatagram("SENDCWID", 8, QHostAddress(Host), PortNum);
}

void ARDOP_GUI::setSend2ToneTest()
{
	udpSocket->writeDatagram("TWOTONETEST", 11, QHostAddress(Host), PortNum);
}

void ARDOP_GUI::rxTimerSlot()
{
	QPalette palette = ui->RXFrame->palette();
	palette.setColor(QPalette::Base, Qt::white);
	ui->RXFrame->setPalette(palette);
	ui->RXFrame->setText("");		// Clear RX Frame
}

void ARDOP_GUI::MyTimerSlot()
{
    // Runs every second

	char Msg[80];
	
	Keepalive++;

    if (Keepalive > 10)
    {
        Keepalive = 0;
        udpSocket->writeDatagram("ARDOP_GUI Running", 17, QHostAddress(Host), PortNum);
    }

	if (GUIActive)
	{
		GUIActive--;
		if (GUIActive == 0)
		{
			sprintf(Msg, "ARDOP_GUI %s:%s", Host, Port);
			this->setWindowTitle(Msg);
		}
	}
}

void ARDOP_GUI::readPendingDatagrams()
{
	while (udpSocket->hasPendingDatagrams())
	{
		QHostAddress Addr;
		quint16 rxPort;
		unsigned char ucopy[1500];
		char copy[1500];

		int Len = udpSocket->readDatagram(copy, 1500, &Addr, &rxPort);

		//		QNetworkDatagram datagram = udpSocket->receiveDatagram();
		//		QByteArray * ba = &datagram.data();
		//		unsigned char copy[1500];
		//     unsigned char * ptr = &copy[1];

		char Msg[1600] = "";

		if (Len > 1500 || Len < 0)
			return;					// ignore it too big

		if (GUIActive == 0)
		{
			sprintf(Msg, "ARDOP_GUI %s:%s Connected", Host, Port);
			this->setWindowTitle(Msg);
		}
		GUIActive = 15;					// Time out after 15 seconds

		copy[Len] = 0;

		memcpy(ucopy, copy, Len);

		switch (copy[0])
		{
		case 'L':					// Signal Level
			RefreshLevel(ucopy[1]);
			sprintf(Msg, "%d %c %d", Len, copy[0], ucopy[1]);
			break;

		case 'D':					// LEDS

			SetLEDS(&ucopy[1]);
			break;

		case 'C':					//Constellation Data
			RefreshConstellation(&ucopy[1], (Len - 1) / 3);		// 3 bytes per pixel x, y, colour
			return;

		case 'X':					// Spectrum display
			RefreshSpectrum(&ucopy[1]);
			return;

		case 'W':					//Waterfall Data
			RefreshWaterfall(&ucopy[1], (Len - 1));
			return;

		case 'S':					// Protocol State
			ui->State->setText(&copy[1]);
			break;

		case 'B':					// Busy state (Sent if no Waterfall or Spectrum)

			if (copy[1] != LastBusy)
			{
				LastBusy = copy[1];
				Busy->setVisible(LastBusy);
			}
			break;

		case 'T':					// TX Frame Type
			ui->TXFrame->setText(&copy[1]);
			break;

		case 'Q':					// Quality
			ui->Quality->setText(&copy[1]);
			break;

		case 'F':					// Frequency
			ui->Frequency->setText(&copy[1]);
			break;

		case 'I':					// Callsign
			ui->Callsign->setText(&copy[1]);
			break;

		case 'R':					// RX Frame Type
		{
			// First Byte is 0/1/2 - Pending/Good/Bad

			int colour = copy[1];

			QPalette palette = ui->RXFrame->palette();

			switch (colour)
			{
			case 0:
				palette.setColor(QPalette::Base, Qt::yellow);
				rxtimer->start(5000);
				break;
			case 1:
				palette.setColor(QPalette::Base, Qt::green);
				rxtimer->start(2000);
				break;
			case 2:
				palette.setColor(QPalette::Base, Qt::red);
				rxtimer->start(2000);
			}
			ui->RXFrame->setPalette(palette);
			ui->RXFrame->setText(&copy[2]);
			break;
		}
		default:
			sprintf(Msg, "%d %c %d", Len, copy[0], ucopy[1]);
			qDebug() << Msg;
		}
	}
}

void  ARDOP_GUI::socketError()
{
    char errMsg[80];
    sprintf(errMsg, "%d %s", udpSocket->state(), udpSocket->errorString().toLocal8Bit().constData());
//	qDebug() << errMsg;
//	QMessageBox::question(NULL, "ARDOP GUI", errMsg, QMessageBox::Yes | QMessageBox::No);
}

void ARDOP_GUI::RefreshLevel(unsigned int Level)
{
    // Redraw the RX Level Bar Graph

    unsigned int  x, y;

    for (x = 0; x < 150; x++)
    {
        for (y = 0; y < 10; y++)
        {
            if (x < Level)
                RXLevel->setPixel(x, y, green);
            else
                RXLevel->setPixel(x, y, white);
        }
    }
    ui->RXLevel->setPixmap(QPixmap::fromImage(*RXLevel));
}

void ARDOP_GUI::RefreshConstellation(unsigned char * Data, int Count)
{
    int i;

    Constellation->fill(black);

    for (i = 0; i < 91; i++)
    {
        Constellation->setPixel(45, i, cyan);
        Constellation->setPixel(i, 45, cyan);
    }

    while (Count--)
    {
        Constellation->setPixel(Data[0], Data[1], vbColours[Data[2]]);
        Data += 3;
    }

    ui->Constellation->setPixmap(QPixmap::fromImage(*Constellation));
}

void ARDOP_GUI::SetLEDS(unsigned char * Data)
{
	if (ui->PTT->isVisible() && Data[0] == 0)
	{
		ui->TXFrame->setText("");			// Clear TX frame when PTT drops
//		ui->RXFrame->setText("");			// ARDOP_WIN does this as well
	}

	ui->PTT->setVisible(Data[0]);
	ui->ISS->setVisible(Data[1]);
	ui->IRS->setVisible(Data[2]);
	ui->TRAFFIC->setVisible(Data[3]);
	ui->ACK->setVisible(Data[4]);
}
void ARDOP_GUI::RefreshSpectrum(unsigned char * Data)
{
	int i;

	// Last 4 bytes are level busy and Tuning lines

	Waterfall->fill(Black);

	if (Data[206] != LastLevel)
	{
		LastLevel = Data[206];
		RefreshLevel(LastLevel);
	}

	if (Data[207] != LastBusy)
	{
		LastBusy = Data[207];
		Busy->setVisible(LastBusy);
	}

	for (i = 0; i < 64; i++)
	{
		Waterfall->setPixel(Data[208], i, Lime);
		Waterfall->setPixel(Data[209], i, Lime);
	}

	for (i = 0; i < 205; i++)
	{
		int val = Data[0];

		if (val > 63)
			val = 63;

		Waterfall->setPixel(i, val, Yellow);
		if (val < 62)
			Waterfall->setPixel(i, val+1, Gold);
		Data++;
	}

	for (i = 0; i < 64; i++)
	{
		Waterfall->setPixel(103, i, Tomato);
	}

	ui->Waterfall->setPixmap(QPixmap::fromImage(*Waterfall));

}

void ARDOP_GUI::RefreshWaterfall(unsigned char * Data, int Count)
{
    int j;
    unsigned char * Line;
    int len = Waterfall->bytesPerLine();
    int TopLine = NextWaterfallLine;

    // Write line to cyclic buffer then draw starting with the line just written

    memcpy(&WaterfallLines[NextWaterfallLine++][0], Data, Count - 2);
    if (NextWaterfallLine > 63)
        NextWaterfallLine = 0;

    for (j = 63; j > 0; j--)
    {
        Line = Waterfall->scanLine(j);
        memcpy(Line, &WaterfallLines[TopLine++][0], len);
        if (TopLine > 63)
            TopLine = 0;
    }

    ui->Waterfall->setPixmap(QPixmap::fromImage(*Waterfall));

	// Next two bytes are level and busy

	if (Data[206] != LastLevel)
	{
		LastLevel = Data[206];
		RefreshLevel(LastLevel);
	}

	if (Data[207] != LastBusy)
	{
		LastBusy = Data[207];
		Busy->setVisible(LastBusy);
	}
}


ARDOP_GUI::~ARDOP_GUI()
{
 //   delete ui;
}
