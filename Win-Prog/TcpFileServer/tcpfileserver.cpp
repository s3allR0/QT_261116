#include "tcpfileserver.h"

TcpFileServer::TcpFileServer(QWidget *parent)
    : QDialog(parent)
{

    totalBytes=0;
    bytesReceived=0;
    fileNameSize=0;

    serverProgressBar=new QProgressBar;
    serverStatusLabel=new QLabel(tr("伺服端就緒"));
    startButton=new QPushButton(tr("接收"));
    quitButton=new QPushButton(tr("退出"));
    buttonBox=new QDialogButtonBox;

    buttonBox->addButton(startButton,QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton,QDialogButtonBox::RejectRole);

    QVBoxLayout *mainLayout =new QVBoxLayout;
    mainLayout -> addWidget(serverProgressBar);
    mainLayout -> addWidget(serverStatusLabel);
    mainLayout -> addStretch();
    mainLayout ->addWidget(buttonBox);
    setLayout(mainLayout);
    setWindowTitle(tr("接收檔案"));
    connect(startButton,SIGNAL(clicked()),this,SLOT(start()));
    connect(quitButton,SIGNAL(clicked()),this,SLOT(close()));
    connect(&tcpServer,SIGNAL(newConnection()),this,SLOT(acceptConnection()));

    connect(&tcpServer,SIGNAL(acceptError(QAbstractSocket::SocketError)),this,
            SLOT(displayError(QAbstractSocket::SocketError)));
}

TcpFileServer::~TcpFileServer() = default;

void TcpFileServer::start(){
    startButton->setEnabled(false);
    bytesReceived=0;
    fileNameSize=0;
    while(!tcpServer.isListening()&&!tcpServer.listen(QHostAddress::AnyIPv4,16689)){
        QMessageBox::StandardButton ret=QMessageBox::critical(this,tr("迴圈"),
                                                                tr("無法起動伺服器:%1.").arg(tcpServer.errorString()),
                                                                QMessageBox::Retry|QMessageBox::Cancel);
        if(ret ==QMessageBox::Cancel)return;
    }
    serverStatusLabel->setText("監聽中...");
}

void TcpFileServer::acceptConnection(){
    tcpServerConnexction=tcpServer.nextPendingConnection();
    connect(tcpServerConnexction,SIGNAL(readyRead()),this,SLOT(updateServerProgress()));
    connect(tcpServerConnexction,SIGNAL(error(QAbstractSocket::socketError)),this,SLOT(displayError(QAbstractSocket::SocketError)));
    serverStatusLabel->setText(tr("接受連線"));
    tcpServer.close();
}

void TcpFileServer::updateServerProgress(){
    QDataStream in(tcpServerConnexction);
    in.setVersion(QDataStream::Qt_4_6);
    if(bytesReceived <= sizeof(qint64)*2){
        if((fileNameSize==0)&&(tcpServerConnexction->bytesAvailable()>=sizeof(qint64)*2)){
            in>>totalBytes>>fileNameSize;
            bytesReceived+=sizeof(qint64)*2;
        }
        if((fileNameSize!=0)&&(tcpServerConnexction->bytesAvailable()>=fileNameSize)){
            in>>fileName;
            bytesReceived+=fileNameSize;
            localFile=new QFile(fileName);
            if(!localFile->open(QFile::WriteOnly)){
                QMessageBox::warning(this,tr("應用程式"),tr("無法讀取檔案%1:\n%2.").arg(fileName).arg(localFile->errorString()));
                return;
            }
        }
        else{
            return;
        }
    }

    if(bytesReceived<totalBytes){
        bytesReceived+=tcpServerConnexction->bytesAvailable();
        inBlock=tcpServerConnexction->readAll();
        localFile->write(inBlock);
        inBlock.resize(0);
    }

    serverProgressBar->setMaximum(totalBytes);
    serverProgressBar->setValue(bytesReceived);
    qDebug()<<bytesReceived;
    serverStatusLabel->setText(tr("已接收% 1 Bytes").arg(bytesReceived));

    if(bytesReceived==totalBytes){
        tcpServerConnexction->close();
        startButton->setEnabled(true);
        localFile->fileName();
        localFile->close();
        start();
    }
}

void TcpFileServer::displayError(QAbstractSocket::SocketError socketError){
    if(socketError==QTcpSocket::RemoteHostClosedError)return;

    QMessageBox::information(this,tr("網路錯誤"),tr("產生以下錯誤: %1.").arg(tcpServerConnexction->errorString()));
    tcpServerConnexction->close();
    serverProgressBar->reset();
    serverStatusLabel->setText(tr("伺服器已就緒"));
    startButton->setEnabled(true);
}