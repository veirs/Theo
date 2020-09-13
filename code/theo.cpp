#include "theo.h"
#include "ui_theo.h"            //version 0.9 dated Sept. 8, 2020
#include <QGraphicsScene>
#include <QImage>
#include <QColor>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include "helpdialog.h"
#include <QPixmap>
#include <stdio.h>
#include "exif.h"
#include <QtMath>

#define RAD2DEG 180.0/3.14159
#define DEG2RAD 3.14159/180.0

Theo::Theo(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Theo)
{
    ui->setupUi(this);
    connect(ui->loadPhoto,SIGNAL(clicked(bool)),this,SLOT(loadPhoto()));
    theoScene = new myGraphicsScene();
    ui->graphicsView->setScene(theoScene);
    thmScene = new myGraphicsScene();
    ui->graphicsView_2->setScene(thmScene);
    connect(theoScene,SIGNAL(mouseSelection(QList<double>)),this,SLOT(gotMouseSelection(QList<double>)));
    connect(ui->priorFile,SIGNAL(clicked(bool)),this,SLOT(retreatOneImage()));
    connect(ui->postFile,SIGNAL(clicked(bool)),this,SLOT(advanceOneImage()));
    connect(ui->clr_1,SIGNAL(clicked(bool)),this,SLOT(clear_1()));
    connect(ui->clr_2,SIGNAL(clicked(bool)),this,SLOT(clear_2()));
    connect(ui->clr_3,SIGNAL(clicked(bool)),this,SLOT(clear_3()));
    connect(ui->clr_4,SIGNAL(clicked(bool)),this,SLOT(clear_4()));
    connect(ui->clr_5,SIGNAL(clicked(bool)),this,SLOT(clear_5()));
    connect(ui->clr_6,SIGNAL(clicked(bool)),this,SLOT(clear_6()));
    connect(ui->clr_7,SIGNAL(clicked(bool)),this,SLOT(clear_7()));

    connect(ui->saveDataDir,SIGNAL(clicked(bool)),this, SLOT(setSaveDir()));
    connect(ui->saveData,SIGNAL(clicked(bool)),this,SLOT(saveData()));
    connect(ui->coord_0,SIGNAL(clicked(bool)),this,SLOT(changeCoord_UTM()));
    connect(ui->coord_1,SIGNAL(clicked(bool)),this,SLOT(changeCoord_LatLong()));

    connect(ui->btnHelp,SIGNAL(clicked()),this,SLOT(showHelp()));
    connect(ui->btnRecalc,SIGNAL(clicked()),this,SLOT(precomputeSpace()));
    connect(ui->btnLocs, SIGNAL(clicked()),this,SLOT(readLocations()));
    PIXxFiducial.resize(4);
    PIXyFiducial.resize(4);
    m_LatLongH.resize(3,5);
    m_xyH.resize(3,5);
    m_xyScene.resize(2,6);
    m_LatLonScene.resize(2,6);
    for (int i=0;i<4;i++){
        PIXxFiducial[i] = 0;
        PIXyFiducial[i] = 0;
    }
    for (int i=0;i<3;i++){
        for (int j = 0;j<6;j++){
            if (i<2){
                m_xyScene[i][j]=0;
                m_LatLonScene[i][j]=0;
            }
            if (j<5){
                m_LatLongH[i][j]=0;
                m_xyH[i][j]=0;
            }
        }
    }

    m_graphicsViewWidth = 0;
    m_targetCnt = 0;
    m_displayUTM = true;

    m_needPrecompute = true;
    m_firstImage = true;  // this is a cheat to shrink the first image after starting the program, then subsequent loads will scale properly
 //   QTextStream(stdout) << "string to print" << endl;  // write directly to console
    m_calibrated = false;

    m_fidBearing.resize(4);
    m_fidRange.resize(4);
    m_fidRange[0] = 999;
    m_fidElevation.resize(4);

    readSettings();

}

Theo::~Theo()
{
    delete ui;
}
void Theo::closeEvent(QCloseEvent * event){
    writeSettings();
}
void Theo::showHelp(){
    HelpDialog mDialog;
    mDialog.setModal(true);
    mDialog.exec();

}
void getLatLong(double x, double y, double &lat, double &lon){
    double Rearth = 6371008.0;
    lat = 48.57235*3.1416/180.0 +(y - 5379952.00)/Rearth;
    lon = -123.26642*3.1416/180.0 + (x - 480347.00)/(Rearth * cos(48.57235*3.1416/180));
    qDebug()<<"UTMs"<<x<<y<<"lat lon"<<QString::number(lat*RAD2DEG, 'f', 10)<<QString::number(lon*RAD2DEG,'f',10)<<QString::number(lat, 'f', 10)<<QString::number(lon,'f',10);
}
void getUTMs(double lat, double lon, double &x, double &y){
    double Rearth = 6371008.0;
    y = (lat - 48.57235*3.1416/180)*Rearth + 5379952;
    x = -(-123.26642*3.1416/180 - lon)*Rearth * cos(48.57235*3.1416/180) + 480347;
}
void Theo::readLocations(){
    QStringList items = ui->photoFolder->text().split("/");
    QString locFolder = "";
    for (int i=0;i<items.length()-2; i++){
        locFolder += items[i]+"/";
    }
    locFolder += "locfiles";

    QString fileName = QFileDialog::getOpenFileName(this,
               tr("Open UTM Location File"), locFolder, tr("Location Files (*.loc)"));

    QFile locFile(fileName);
    locFile.open(QIODevice::ReadOnly);
    if (!locFile.isOpen())
        return;
    items = fileName.split("/");
    ui->locFilename->setText(items.last());
    items = fileName.split(".");
    QString thumbnailFilename = items[0]+".jpg";
    QImage img;
    img.load(thumbnailFilename);

    qDebug()<<thumbnailFilename;
    g_pixmap_thm = QPixmap::fromImage(img).scaledToWidth(329);
    ui->graphicsView_2->scene()->addPixmap(g_pixmap_thm);

    QTextStream stream(&locFile);
    QString line = stream.readLine(); //this is header line
    line = stream.readLine();
    while (!line.isNull()) {
        /* process information */
        QStringList items = line.split("\t");
        if (items[0] == "camera"){
            ui->camera_text->setText(items[1]);
            ui->xCamera->setValue(items[2].toDouble());
            ui->yCamera->setValue(items[3].toDouble());
            ui->zCamera->setValue(items[4].toDouble());
        }
        if (items[0] == "fiducial_1"){
            ui->fiducial_text_0->setText(items[1]);
            ui->xFiducial_0->setValue(items[2].toDouble());
            ui->yFiducial_0->setValue(items[3].toDouble());
            ui->zFiducial_0->setValue(items[4].toDouble());
        }
        if (items[0] == "fiducial_2"){
            ui->fiducial_text_1->setText(items[1]);
            ui->xFiducial_1->setValue(items[2].toDouble());
            ui->yFiducial_1->setValue(items[3].toDouble());
            ui->zFiducial_1->setValue(items[4].toDouble());
        }
        if (items[0] == "fiducial_3"){
            ui->fiducial_text_2->setText(items[1]);
            ui->xFiducial_2->setValue(items[2].toDouble());
            ui->yFiducial_2->setValue(items[3].toDouble());
            ui->zFiducial_2->setValue(items[4].toDouble());
        }
        if (items[0] == "fiducial_4"){
            ui->fiducial_text_3->setText(items[1]);
            ui->xFiducial_3->setValue(items[2].toDouble());
            ui->yFiducial_3->setValue(items[3].toDouble());
            ui->zFiducial_3->setValue(items[4].toDouble());
        }
        line = stream.readLine();
//        qDebug()<<line;
    }
    m_needDigitizeFids = true;
    initializeGlobals();
}
void Theo::initializeGlobals(){
    // update from Theo form
    m_xyH[0][0]=ui->xCamera->value();
    m_xyH[1][0]=ui->yCamera->value();
    m_xyH[2][0]=ui->zCamera->value();

    m_xyH[0][1]=ui->xFiducial_0->value();
    m_xyH[0][2]=ui->xFiducial_1->value();
    m_xyH[0][3]=ui->xFiducial_2->value();
    m_xyH[0][4]=ui->xFiducial_3->value();
    m_xyH[1][1]=ui->yFiducial_0->value();
    m_xyH[1][2]=ui->yFiducial_1->value();
    m_xyH[1][3]=ui->yFiducial_2->value();
    m_xyH[1][4]=ui->yFiducial_3->value();
    m_xyH[2][1]=ui->zFiducial_0->value();
    m_xyH[2][2]=ui->zFiducial_1->value();
    m_xyH[2][3]=ui->zFiducial_2->value();
    m_xyH[2][4]=ui->zFiducial_3->value();
    // calculate lats and lons
    calculateLatLonFromUTMs();
    // Calculate ranges to fiducials
    ////////////////////////  Initialize range and bearing and elevaton from fiducial to camera
    m_fidRange[0] = sqrt(SQR(m_xyH[0][0] -m_xyH[0][1]) + SQR(m_xyH[1][0] -m_xyH[1][1]) + SQR(m_xyH[2][0] -m_xyH[2][1]));
    m_fidRange[1] = sqrt(SQR(m_xyH[0][0] -m_xyH[0][2]) + SQR(m_xyH[1][0] -m_xyH[1][2]) + SQR(m_xyH[2][0] -m_xyH[2][2]));
    m_fidRange[2] = sqrt(SQR(m_xyH[0][0] -m_xyH[0][3]) + SQR(m_xyH[1][0] -m_xyH[1][3]) + SQR(m_xyH[2][0] -m_xyH[2][3]));
    m_fidRange[3] = sqrt(SQR(m_xyH[0][0] -m_xyH[0][4]) + SQR(m_xyH[1][0] -m_xyH[1][4]) + SQR(m_xyH[2][0] -m_xyH[2][4]));

    ui->R_0->setValue(m_fidRange[0]);ui->R_1->setValue(m_fidRange[1]);ui->R_2->setValue(m_fidRange[2]);ui->R_3->setValue(m_fidRange[3]);

    for (int i=0;i<4;i++){
//        double bearing = getBearingFromLatLong(i+1);
        double bearing = atan2( m_xyH[0][i+1] - m_xyH[0][0], m_xyH[1][i+1] - m_xyH[1][0]) ;
        if (bearing < 0) bearing += 2*3.1416;
        m_fidBearing[i] = bearing;

        m_fidElevation[i] = atan2(m_xyH[2][0] - m_xyH[2][i+1], m_fidRange[i]);
        if (m_fidElevation[i] < 0){
            m_fidElevation[i] += 2*3.1416;
        }
        qDebug()<<"Elevation angles from fid"<<i<<m_fidElevation[i]*RAD2DEG<<" bearing to fid"<<m_fidBearing[i]*RAD2DEG;
    }
    ui->Az_0->setValue(m_fidBearing[0]*RAD2DEG);ui->Az_1->setValue(m_fidBearing[1]*RAD2DEG);ui->Az_2->setValue(m_fidBearing[2]*RAD2DEG);ui->Az_3->setValue(m_fidBearing[3]*RAD2DEG);
    m_needPrecompute = true;
}
void Theo::calculateLatLonFromUTMs(){
    // construct latlongH from UTMs that were saved
    double lat; double lon;
    for (int i=0;i<5;i++){
        if (m_xyH[0][i] > 0){
            getLatLong(m_xyH[0][i], m_xyH[1][i],lat,lon);
            m_LatLongH[0][i]=lat; m_LatLongH[1][i]=lon; m_LatLongH[2][i]=m_xyH[2][i];
        } else {
            m_LatLongH[0][i]=m_LatLongH[1][i]= m_LatLongH[2][i]=0;
        }
    }
}
void Theo::calculateRadPerPixel(){
    // Calculate radians per pixel
    double d_14 = sqrt(SQR(m_xyH[0][1] -m_xyH[0][4]) + SQR(m_xyH[1][1] -m_xyH[1][4]) + SQR(m_xyH[2][1] -m_xyH[2][4]));
    double pix_AR = sqrt(SQR(PIXxFiducial[0]-PIXxFiducial[3]) + SQR(PIXyFiducial[0]-PIXyFiducial[3]));  // distance between fiducial 1 and 4 in PIXELS on screen
    double alpha = acos((SQR(d_14) -SQR(m_fidRange[0]) - SQR(m_fidRange[3]))/(-2.0*m_fidRange[0]*m_fidRange[3]));
    m_radPerPixel = alpha/pix_AR;  // radians per pixel
}
void Theo::calculateCameraBearingAz(){
    // calculate radians per pixel
    calculateRadPerPixel();

    // calculate FOV x and FOV y and camera bearing
    double phiCam;
    m_ccd_x = ui->CCD_x->value();
    m_ccd_y = ui->CCD_y->value();
    ui->fov_X->setValue(m_ccd_x*m_radPerPixel*RAD2DEG);
    ui->fov_Y->setValue(m_ccd_y*m_radPerPixel*RAD2DEG);
    m_fov_x = m_ccd_x*m_radPerPixel;
    m_fov_y = m_ccd_y*m_radPerPixel;
    phiCam = calcCameraAz(ui->fov_X->value(),ui->CCD_x->value());
    ui->AzCam->setValue(phiCam*RAD2DEG);
    m_azimuth_cam = phiCam;
    // calculate camera tilt
    VecDoub epsilon(3);
    double epsilonAve = 0;
    for (int i=0;i<4;i++){  // averaging camera tilt from THREE fids
        double theta = m_fidElevation[i];
        double epsilon = theta -(PIXyFiducial[i] - ui->CCD_y->value()/2.0)*m_radPerPixel;
        if (i < 3)
            epsilonAve += epsilon;
        qDebug()<<"elevation from fid "<<i<<"is"<<epsilon*RAD2DEG;
    }
    epsilonAve /= 3.0;   //  Note Bene for camera tilt,  we are ignoring any contribution from the close fid #4
    qDebug()<<"camera tilt ="<<epsilonAve*RAD2DEG;
    ui->tiltCam->setValue(epsilonAve*RAD2DEG);
    m_tilt_cam = epsilonAve;
}


int getIdxMatchingUTM(MatDoub utmImg, int yHorizon, int midXasix, int xStep, int &thisStep, double thisUTM){   //m_UTMx_Img, yHorizon, axisIdx_x, thisUTM
    int idxY = yHorizon;  // return y value on vertical line offset by xStep where utmImg[][] 'close' to thisUTM
    thisStep = xStep;
    if (utmImg[midXasix+thisStep][idxY] > thisUTM){
        while ((utmImg[midXasix+thisStep][idxY] > thisUTM) && (idxY>0)){
            idxY++;
        }
        if (idxY == 0){
            idxY = yHorizon;
            while ((utmImg[midXasix-thisStep][idxY] > thisUTM) && (idxY>0)){
                idxY++;
            }
        }
    } else {
        thisStep = -xStep;
            while ((utmImg[midXasix+thisStep][idxY] < thisUTM) && (idxY>0)){
                idxY++;
            }
            if (idxY == 0){
                idxY = yHorizon;
                while ((utmImg[midXasix+thisStep][idxY] < thisUTM) && (idxY>0)){
                    idxY++;
                }
        }
    }
    //qDebug()<<" in getIdxMatchingUTM"<<thisUTM<<utmImg[midXasix-thisStep][idxY]<<idxY;
    return idxY;
}
void Theo::extrap(MatDoub utms, int idx1, int idy1, int idx2, int idy2, int &i1, int &j1, int &i2, int &j2){ //extrap(m_UTMx_Img,x,y,x+thisStep,IdxY,i1,i2,j1,j2);
    // extrapolate line to edjes and return indices
    i1 = idx1; j1 = idy1;
    i2 = idx2; j2 = idy2;
    int i;
    int j;
    double m = (float)(idy2-idy1)/(float)(idx2-idx1);
    i = idx1;
    j = idy1;

    while ((i>0) && (i < ui->CCD_x->value()) && (j>0) && (j < ui->CCD_y->value()) && (utms[i][j] != -9999) ){
        j = j1 + m*(i - i1);
//        qDebug()<<"outward"<<i<<j<<utms[i][j];
        i++;
    }
    i1 = i; j1 = j;
    i = idx1;
    j = idy1;
    while ((i>0) && (i < ui->CCD_x->value()) && (j>0) && (j < ui->CCD_y->value()) && (utms[i][j] != -9999) ){
        j = j1 + m*(i - i1);
        i--;
    }
    i2 = i;
    j2 = j;
}

void Theo::precomputeSpace(){
    this->setCursor(Qt::WaitCursor);
    // calculate the range and bearing from each pixel in image to location of camera.
    //     OR  calculate the UTMx and UTMy of each pont i image -- then calculate range and bearing later
    int Ny = ui->CCD_y->value();
    int Nx = ui->CCD_x->value();
    initializeGlobals();
    calculateCameraBearingAz();

    VecDoub Y(Ny);
    MatDoub X(Nx, Ny);
    bool gotOne = false;
    qDebug()<<"camera tilt"<<m_tilt_cam*DEG2RAD;
    for (int j=0; j<Ny; j++){
        double theta_Target = m_tilt_cam + ((float)j-0.5*m_ccd_y)*m_radPerPixel;
        double range = m_xyH[2][0]/tan(theta_Target);
        Y[j] = range;
        if (range < 0) Y[j] = -9999;   // Note Bene Do we need this max range limit?????
    }
    gotOne = false;
    for (int j=0;j<Ny;j++){
        for (int i=0;i<(int)(Nx/2);i++){
            double f = ((float)Nx/2.0 - (float)i)*2.0/(float)Nx;
            X[i][j] = -Y[j]*sin(f*m_fov_x/2.0);  // note earlier version had f outside sin but almost no difference in x values
            X[Nx-i-1][j] = - X[i][j];  // camera axis splits the ccd and hence level camera is symmetric in locations relative to the center of the ccd
        }
    }

// now transform into utm coordinates
    // X and Y are relative to camera with Y pointing away and X to the right
    // Rotate about vertical axis at the point where lens axis strikes the sea
    m_UTMx_Img.resize(Nx,Ny);
    m_UTMy_Img.resize(Nx,Ny);
    m_UTMx_ContourIdx.resize(Nx,Ny);
    m_UTMy_ContourIdx.resize(Nx,Ny);
    double UTMxAxis; double UTMyAxis;

    double RtoAxis = m_xyH[2][0]/tan(m_tilt_cam);  // ht of camera / tan elevation

    UTMxAxis = ui->xCamera->value()+RtoAxis*sin(m_azimuth_cam);
    UTMyAxis = ui->yCamera->value()+RtoAxis*cos(m_azimuth_cam);
    qDebug()<<"RtoAxis"<<RtoAxis<<"UTMxAxis"<<UTMxAxis<<"UTMyAxis"<<UTMyAxis<<"sin(m_azimuth_cam)"<<sin(m_azimuth_cam)<<"cos(m_azimuth_cam)"<<cos(m_azimuth_cam);
    int cnt = 0;
    double maxUTMx = 0; double minUTMx = 9e9;
    double maxUTMy = 0; double minUTMy = 9e9;
    int axisIdx_x; int axisIdx_y;
    double minDist = 9e9;
    for (int j=0; j<Ny; j++){
        for (int i=0; i<Nx; i++){
            if (Y[j]==-9999 || X[i][j]==-9999){
                m_UTMx_Img[i][j] = m_UTMy_Img[i][j] = -9999;  // no UTM coordinates at this image location
            } else {
                double utmx = UTMxAxis + (X[i][j]*cos(m_azimuth_cam) + (Y[j]-RtoAxis)*sin(m_azimuth_cam));
                double utmy = UTMyAxis + (-X[i][j]*sin(m_azimuth_cam) + (Y[j]-RtoAxis)*cos(m_azimuth_cam));
                double dist = SQR(utmx - UTMxAxis) + SQR(utmy - UTMyAxis);
                if (dist < minDist){
                    minDist = dist;
                    axisIdx_x = i;
                    axisIdx_y = j;
                }
                m_UTMx_Img[i][j] = utmx;
                m_UTMy_Img[i][j] = utmy;
                maxUTMx = max(maxUTMx,utmx);
                maxUTMy = max(maxUTMy,utmy);
                minUTMx = min(minUTMx,utmx);
                minUTMy = min(minUTMy,utmy);
            }
        }
    }


    int Ncontours = 5;
    m_utmXline_px_x.resize(2,Ncontours);
    m_utmXline_px_y.resize(2,Ncontours);
    m_utmYline_px_x.resize(2,Ncontours);
    m_utmYline_px_y.resize(2,Ncontours);

    VecDoub xLineUTMs(Ncontours);
    VecDoub yLineUTMs(Ncontours);

    // axisIdx_y is y axis pixel value of point where camera axis penetrates z=0 plane

    // chose Ncontours points in image's vertical direction for utmx and utmy isolines
    int yHorizon = 0;
    while (((m_UTMx_Img[axisIdx_x][yHorizon] == -9999) && (m_UTMy_Img[axisIdx_y][yHorizon] == -9999)) && (yHorizon < m_ccd_y)){
        yHorizon++;
    }
    int deltay = (axisIdx_y - yHorizon)/(Ncontours-1);
    int deltaX = 100;  // step this much  to the side to find other point on UTM isoline
    int i1; int j1;int i2;int j2;
    for (int k=0; k<Ncontours;k++){
        int x = axisIdx_x;
        int y = yHorizon + deltay*0.25 + deltay*(k);
        // step sideways and find points that match the current location's utms
        int IdxY;
        int thisStep;
        IdxY = getIdxMatchingUTM(m_UTMx_Img, yHorizon, axisIdx_x,  deltaX, thisStep, m_UTMx_Img[x][y]);
        extrap(m_UTMx_Img,x,y,x+thisStep,IdxY,i1,j1,i2,j2);
        m_utmXline_px_x[0][k] = i1;
        m_utmXline_px_y[0][k] = j1;
        m_utmXline_px_x[1][k] = i2;
        m_utmXline_px_y[1][k] = j2;

        IdxY = getIdxMatchingUTM(m_UTMy_Img, yHorizon, axisIdx_x, deltaX, thisStep, m_UTMy_Img[x][y]);
        extrap(m_UTMy_Img,x,y,x+thisStep,IdxY,i1,j1,i2,j2);
        m_utmYline_px_x[0][k] = i1;
        m_utmYline_px_y[0][k] = j1;
        m_utmYline_px_x[1][k] = i2;
        m_utmYline_px_y[1][k] = j2;

    }


    QString fullPathFile = ui->photoFolder->text()+ui->photoFilename->text();
    bool drawLines = false;
    if (ui->parm->value() == 10) drawLines = true;
    loadTheImage(fullPathFile,true,drawLines);  // true here says rescaleToView
    this->setCursor(Qt::ArrowCursor);
    m_needPrecompute = false;
}


void Theo::readSettings(){

    QSettings settings("Theo_1");
    bool init = settings.value("init",false).toBool();
    if (!init)
        return;
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(800, 500)).toSize();
    resize(size);
    move(pos);
    ui->xCamera->setDecimals(1);ui->yCamera->setDecimals(1);
    ui->xCamera->setValue(settings.value("xcam",487219).toDouble());
    ui->yCamera->setValue(settings.value("ycam",5378386).toDouble());
    ui->zCamera->setValue(settings.value("zcam",11).toDouble());
    ui->camera_text->setText(settings.value("textcam","OrcaSound").toString());


    ui->xFiducial_0->setDecimals(1);ui->yFiducial_0->setDecimals(1);
    ui->xFiducial_1->setDecimals(1);ui->yFiducial_1->setDecimals(1);
    ui->xFiducial_2->setDecimals(1);ui->yFiducial_2->setDecimals(1);
    ui->xFiducial_3->setDecimals(1);ui->yFiducial_3->setDecimals(1);


    ui->xFiducial_0->setValue(settings.value("xFiducial_0",479563).toDouble());
    ui->xFiducial_1->setValue(settings.value("xFiducial_1",479491).toDouble());
    ui->xFiducial_2->setValue(settings.value("xFiducial_2",480047).toDouble());
    ui->xFiducial_3->setValue(settings.value("xFiducial_3",487205).toDouble());

    ui->yFiducial_0->setValue(settings.value("yFiducial_0",5378664).toDouble());
    ui->yFiducial_1->setValue(settings.value("yFiducial_1",5379825).toDouble());
    ui->yFiducial_2->setValue(settings.value("yFiducial_2",5385183).toDouble());
    ui->yFiducial_3->setValue(settings.value("yFiducial_3",5378407).toDouble());

    m_xyH[0][0]=ui->xCamera->value();
    m_xyH[1][0]=ui->yCamera->value();
    m_xyH[2][0]=ui->zCamera->value();

    ui->yFiducial_0->setValue(settings.value("yFiducial_0",5378664).toDouble());
    ui->yFiducial_1->setValue(settings.value("yFiducial_1",5379825).toDouble());
    ui->yFiducial_2->setValue(settings.value("yFiducial_2",5385183).toDouble());
    ui->yFiducial_3->setValue(settings.value("yFiducial_3",5378407).toDouble());

    ui->zFiducial_0->setValue(settings.value("zFiducial_0",0).toDouble());
    ui->zFiducial_1->setValue(settings.value("zFiducial_1",0).toDouble());
    ui->zFiducial_2->setValue(settings.value("zFiducial_2",0).toDouble());
    ui->zFiducial_3->setValue(settings.value("zFiducial_3",3).toDouble());

    ui->fiducial_text_0->setText(settings.value("fiducial_text_0","Darcy South").toString());
    ui->fiducial_text_1->setText(settings.value("fiducial_text_1","Darcy North").toString());
    ui->fiducial_text_2->setText(settings.value("fiducial_text_2","The Rock").toString());
    ui->fiducial_text_3->setText(settings.value("fiducial_text_3","").toString());

    ui->locFilename->setText(settings.value("locFile","No Locfile").toString());
    ui->photoFolder->setText(settings.value("photoFolder","/home/val/Qt_Projects/raspberryPi/").toString());
    ui->saveFolder->setText(settings.value("saveFolder","/home/val/Qt_Projects/raspberryPi/").toString());

    PIXxFiducial[0] = settings.value("pixx_0").toDouble();
    PIXyFiducial[0] = settings.value("pixy_0").toDouble();
    PIXxFiducial[1] = settings.value("pixx_1").toDouble();
    PIXyFiducial[1] = settings.value("pixy_1").toDouble();
    PIXxFiducial[2] = settings.value("pixx_2").toDouble();
    PIXyFiducial[2] = settings.value("pixy_2").toDouble();
    PIXxFiducial[3] = settings.value("pixx_3").toDouble();
    PIXyFiducial[3] = settings.value("pixy_3").toDouble();
    m_graphicsViewWidth = settings.value("gviewwd").toInt();

    m_displayUTM = settings.value("displayUTM",true).toBool();
    // initialize globals
    initializeGlobals();
    // construct latlongH from UTMs that were saved
    calculateLatLonFromUTMs();
}
void Theo::writeSettings(){
    if (!m_displayUTM)
        changeCoord_UTM();
    QSettings settings("Theo_1");
    qDebug()<<ui->initChk->isChecked();
    settings.setValue("init", ui->initChk->isChecked());
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.setValue("xcam",ui->xCamera->value());
    settings.setValue("ycam",ui->yCamera->value());
    settings.setValue("zcam",ui->zCamera->value());
    settings.setValue("textcam",ui->camera_text->text());
    settings.setValue("xFiducial_0",ui->xFiducial_0->value());
    settings.setValue("xFiducial_1",ui->xFiducial_1->value());
    settings.setValue("xFiducial_2",ui->xFiducial_2->value());
    settings.setValue("xFiducial_3",ui->xFiducial_3->value());

    settings.setValue("yFiducial_0",ui->yFiducial_0->value());
    settings.setValue("yFiducial_1",ui->yFiducial_1->value());
    settings.setValue("yFiducial_2",ui->yFiducial_2->value());
    settings.setValue("yFiducial_3",ui->yFiducial_3->value());
\

    settings.setValue("zFiducial_0",ui->zFiducial_0->value());
    settings.setValue("zFiducial_1",ui->zFiducial_1->value());
    settings.setValue("zFiducial_2",ui->zFiducial_2->value());
    settings.setValue("zFiducial_3",ui->zFiducial_3->value());

    settings.setValue("fiducial_text_0",ui->fiducial_text_0->text());
    settings.setValue("fiducial_text_1",ui->fiducial_text_1->text());
    settings.setValue("fiducial_text_2",ui->fiducial_text_2->text());
    settings.setValue("fiducial_text_3",ui->fiducial_text_3->text());

    settings.setValue("photoFolder",ui->photoFolder->text());
    settings.setValue("saveFolder",ui->saveFolder->text());
    settings.setValue("locFile",ui->locFilename->text());

    settings.setValue("pixx_0",PIXxFiducial[0]);
    settings.setValue("pixy_0",PIXyFiducial[0]);
    settings.setValue("pixx_1",PIXxFiducial[1]);
    settings.setValue("pixy_1",PIXyFiducial[1]);
    settings.setValue("pixx_2",PIXxFiducial[2]);
    settings.setValue("pixy_2",PIXyFiducial[2]);
    settings.setValue("pixx_3",PIXxFiducial[3]);
    settings.setValue("pixy_3",PIXyFiducial[3]);

    settings.setValue("gviewwd",m_graphicsViewWidth);
    settings.setValue("displayUTM",m_displayUTM);
}
void Theo::changeCoord_LatLong(){
    if (m_displayUTM)
        m_displayUTM = false;
//    ui->label_2->setText("Lat");
//    ui->label_3->setText("Long");
//    ui->label_9->setText("Lat");
//    ui->label_10->setText("Long");
    ui->xCamera->setDecimals(4);ui->yCamera->setDecimals(4);
    ui->xCamera->setValue(m_LatLongH[0][0]*RAD2DEG);
    ui->yCamera->setValue(m_LatLongH[1][0]*RAD2DEG);
    ui->zCamera->setValue(m_LatLongH[2][0]);
    ui->xFiducial_0->setDecimals(4); ui->yFiducial_0->setDecimals(4);
    ui->xFiducial_1->setDecimals(4); ui->yFiducial_1->setDecimals(4);
    ui->xFiducial_2->setDecimals(4); ui->yFiducial_2->setDecimals(4);
    ui->xFiducial_3->setDecimals(4); ui->yFiducial_3->setDecimals(4);

    ui->xFiducial_0->setValue(m_LatLongH[0][1]*RAD2DEG);
    ui->yFiducial_0->setValue(m_LatLongH[1][1]*RAD2DEG);
    ui->zFiducial_0->setValue(m_LatLongH[2][1]);
    ui->xFiducial_1->setValue(m_LatLongH[0][2]*RAD2DEG);
    ui->yFiducial_1->setValue(m_LatLongH[1][2]*RAD2DEG);
    ui->zFiducial_1->setValue(m_LatLongH[2][2]);
    ui->xFiducial_2->setValue(m_LatLongH[0][3]*RAD2DEG);
    ui->yFiducial_2->setValue(m_LatLongH[1][3]*RAD2DEG);
    ui->zFiducial_2->setValue(m_LatLongH[2][3]);
    ui->xFiducial_3->setValue(m_LatLongH[0][4]*RAD2DEG);
    ui->yFiducial_3->setValue(m_LatLongH[1][4]*RAD2DEG);
    ui->zFiducial_3->setValue(m_LatLongH[2][4]);

}
void Theo::changeCoord_UTM(){
    if (!m_displayUTM)
        m_displayUTM = true;
//    ui->label_2->setText("UTMx");
//    ui->label_3->setText("UTMy");
//    ui->label_9->setText("UTMx");
//    ui->label_10->setText("UTMy");
    ui->xCamera->setDecimals(1);ui->yCamera->setDecimals(1);
    ui->xFiducial_0->setDecimals(1);ui->yFiducial_0->setDecimals(1);
    ui->xFiducial_1->setDecimals(1);ui->yFiducial_1->setDecimals(1);
    ui->xFiducial_2->setDecimals(1);ui->yFiducial_2->setDecimals(1);
    ui->xFiducial_3->setDecimals(1);ui->yFiducial_3->setDecimals(1);

    ui->xCamera->setValue(m_xyH[0][0]);
    ui->yCamera->setValue(m_xyH[1][0]);
    ui->zCamera->setValue(m_xyH[2][0]);
    ui->xFiducial_0->setValue(m_xyH[0][1]);
    ui->yFiducial_0->setValue(m_xyH[1][1]);
    ui->zFiducial_0->setValue(m_xyH[2][1]);
    ui->xFiducial_1->setValue(m_xyH[0][2]);
    ui->yFiducial_1->setValue(m_xyH[1][2]);
    ui->zFiducial_1->setValue(m_xyH[2][2]);
    ui->xFiducial_2->setValue(m_xyH[0][3]);
    ui->yFiducial_2->setValue(m_xyH[1][3]);
    ui->zFiducial_2->setValue(m_xyH[2][3]);
    ui->xFiducial_3->setValue(m_xyH[0][4]);
    ui->yFiducial_3->setValue(m_xyH[1][4]);
    ui->zFiducial_3->setValue(m_xyH[2][4]);
}
void Theo::setSaveDir(){
    QString curDir = ui->saveFolder->text();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                     curDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    ui->saveFolder->setText(dir);
}
void Theo::saveData(){
    QString saveDir = ui->saveFolder->text();
    QStringList items = saveDir.split("/");
    if (items.last() != ""){
        saveDir = saveDir +"/";
    }
    QString fileName = QFileDialog::getOpenFileName(this,
               tr("Open Theo data file"), saveDir, tr("Theo Files (*.theo)"));
    bool newFile = false;
    if (fileName == ""){
        fileName = saveDir + ui->dataFilename->text();
        newFile = true;
    }
    if (ui->dataFilename->text() == "")
        fileName = saveDir+"testData.theo";
    items = fileName.split("/");
    ui->dataFilename->setText(items.last());
    QFile *saveFile = new QFile(fileName);
    if (!saveFile->open(QIODevice::ReadWrite | QIODevice::Append | QIODevice::Text)){
        QMessageBox msgBox;
        msgBox.setText("Cannot open theo file");
        msgBox.exec();
        return;
    }
    // if a new data file put in header info
    QTextStream OUT(saveFile);
    if (newFile){
        QString header="photoFilename\tdate\ttime\tUTMx\tUTMy\tRange\tAzimuth\tLoc_File\tComment\n";
        OUT << header;
    }
    QString outLine;
    QString theDate = "no_date";
    QString theTime = "no_time";
    if (ui->xScene_1->value() !=0){items=m_photoDate[0].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else{
            QString theDate = "no_date";
            QString theTime = "no_time";
        }
        outLine = m_photoFilename[0]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_1->value(),'f',2)+"\t"+QString::number(ui->yScene_1->value(),'f',2)+"\t"+
                QString::number(ui->Range_1->value())+"\t"+QString::number(ui->Bearing_1->value())+"\t"+m_locFilename[0]+"\t"+ui->Comment_1->text()+"\n";
        OUT << outLine;
    }
    if (ui->xScene_2->value() !=0){items=m_photoDate[1].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else{
            QString theDate = "no_date";
            QString theTime = "no_time";
        }
        outLine = m_photoFilename[1]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_2->value(),'f',2)+"\t"+QString::number(ui->yScene_2->value(),'f',2)+"\t"+
                QString::number(ui->Range_2->value())+"\t"+QString::number(ui->Bearing_2->value())+"\t"+m_locFilename[1]+"\t"+ui->Comment_2->text()+"\n";
        OUT << outLine;
    }
    if (ui->xScene_3->value() !=0){items=m_photoDate[2].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else{
            QString theDate = "no_date";
            QString theTime = "no_time";
        }
        outLine = m_photoFilename[2]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_3->value(),'f',2)+"\t"+QString::number(ui->yScene_3->value(),'f',2)+"\t"+
                QString::number(ui->Range_3->value())+"\t"+QString::number(ui->Bearing_3->value())+"\t"+m_locFilename[2]+"\t"+ui->Comment_3->text()+"\n";
        OUT << outLine;
    }
    if (ui->xScene_4->value() !=0){items=m_photoDate[3].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else{
            QString theDate = "no_date";
            QString theTime = "no_time";
        }
        outLine = m_photoFilename[3]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_4->value(),'f',2)+"\t"+QString::number(ui->yScene_4->value(),'f',2)+"\t"+
                QString::number(ui->Range_4->value())+"\t"+QString::number(ui->Bearing_4->value())+"\t"+m_locFilename[3]+"\t"+ui->Comment_4->text()+"\n";
        OUT << outLine;
    }
    if (ui->xScene_5->value() !=0){items=m_photoDate[4].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else{
            QString theDate = "no_date";
            QString theTime = "no_time";
        }
        outLine = m_photoFilename[4]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_5->value(),'f',2)+"\t"+QString::number(ui->yScene_5->value(),'f',2)+"\t"+
                QString::number(ui->Range_5->value())+"\t"+QString::number(ui->Bearing_5->value())+"\t"+m_locFilename[4]+"\t"+ui->Comment_5->text()+"\n";
        OUT << outLine;
    }
    if (ui->xScene_6->value() !=0){items=m_photoDate[5].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else;{
            QString theDate = "no_date";
            QString theTime = "no time";
        }
        outLine = m_photoFilename[5]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_6->value(),'f',2)+"\t"+QString::number(ui->yScene_6->value(),'f',2)+"\t"+
                QString::number(ui->Range_6->value())+"\t"+QString::number(ui->Bearing_6->value())+"\t"+m_locFilename[5]+"\t\""+ui->Comment_6->text()+"\"\n";
        OUT << outLine;
    }
    if (ui->xScene_7->value() !=0){items=m_photoDate[6].split(" ");
        if (items.length() == 2){
            theDate = items[0];
            theTime = items[1];
        }else;{
            QString theDate = "no_date";
            QString theTime = "no time";
        }
        outLine = m_photoFilename[6]+"\t"+theDate+"\t"+theTime+"\t"+QString::number(ui->xScene_7->value(),'f',2)+"\t"+QString::number(ui->yScene_7->value(),'f',2)+"\t"+
                QString::number(ui->Range_7->value())+"\t"+QString::number(ui->Bearing_7->value())+"\t"+m_locFilename[6]+"\t\""+ui->Comment_7->text()+"\"\n";
        OUT << outLine;
    }

    saveFile->close();
}

void Theo::clear_1(){
    ui->xScene_1->setValue(0);ui->yScene_1->setValue(0);ui->Comment_1->setText("");ui->Range_1->setValue(0);ui->Bearing_1->setValue(0);m_photoDate[0]="";m_photoFilename[0]="";m_locFilename[0]="";
    compressTargetItems(1);
}
void Theo::clear_2(){
    ui->xScene_2->setValue(0);ui->yScene_2->setValue(0);ui->Comment_2->setText("");ui->Range_2->setValue(0);ui->Bearing_2->setValue(0);m_photoDate[1]="";m_photoFilename[1]="";m_locFilename[1]="";
    compressTargetItems(2);
}
void Theo::clear_3(){
    ui->xScene_3->setValue(0);ui->yScene_3->setValue(0);ui->Comment_3->setText("");ui->Range_3->setValue(0);ui->Bearing_3->setValue(0);m_photoDate[2]="";m_photoFilename[2]="";m_locFilename[2]="";
    compressTargetItems(3);
}
void Theo::clear_4(){
    ui->xScene_4->setValue(0);ui->yScene_4->setValue(0);ui->Comment_4->setText("");ui->Range_4->setValue(0);ui->Bearing_4->setValue(0);m_photoDate[3]="";m_photoFilename[3]="";m_locFilename[3]="";
    compressTargetItems(4);
}
void Theo::clear_5(){
    ui->xScene_5->setValue(0);ui->yScene_5->setValue(0);ui->Comment_5->setText("");ui->Range_5->setValue(0);ui->Bearing_5->setValue(0);m_photoDate[4]="";m_photoFilename[4]="";m_locFilename[4]="";
    compressTargetItems(5);
}
void Theo::clear_6(){
    ui->xScene_6->setValue(0);ui->yScene_6->setValue(0);ui->Comment_6->setText("");ui->Range_6->setValue(0);ui->Bearing_6->setValue(0);m_photoDate[5]="";m_photoFilename[5]="";m_locFilename[5]="";
    compressTargetItems(6);
}
void Theo::clear_7(){
    ui->xScene_7->setValue(0);ui->yScene_7->setValue(0);ui->Comment_7->setText("");ui->Range_7->setValue(0);ui->Bearing_7->setValue(0);m_photoDate[6]="";m_photoFilename[6]="";m_locFilename[6]="";
    compressTargetItems(7);
}
void Theo::compressTargetItems(int idx){

    if (idx <=1) {ui->xScene_1->setValue(ui->xScene_2->value());ui->yScene_1->setValue(ui->yScene_2->value());ui->Comment_1->setText(ui->Comment_2->text());ui->Range_1->setValue(ui->Range_2->value());ui->Bearing_1->setValue(ui->Bearing_2->value());m_photoDate[0]=m_photoDate[1];m_photoFilename[0] = m_photoFilename[1];m_locFilename[0]=m_locFilename[1];}
    if (idx <=2) {ui->xScene_2->setValue(ui->xScene_3->value());ui->yScene_2->setValue(ui->yScene_3->value());ui->Comment_2->setText(ui->Comment_3->text());ui->Range_2->setValue(ui->Range_3->value());ui->Bearing_2->setValue(ui->Bearing_3->value());m_photoDate[1]=m_photoDate[2];m_photoFilename[1] = m_photoFilename[2];m_locFilename[1]=m_locFilename[2];}
    if (idx <=3) {ui->xScene_3->setValue(ui->xScene_4->value());ui->yScene_3->setValue(ui->yScene_4->value());ui->Comment_3->setText(ui->Comment_4->text());ui->Range_3->setValue(ui->Range_4->value());ui->Bearing_3->setValue(ui->Bearing_4->value());m_photoDate[2]=m_photoDate[3];m_photoFilename[2] = m_photoFilename[3];m_locFilename[2]=m_locFilename[3];}
    if (idx <=4) {ui->xScene_4->setValue(ui->xScene_5->value());ui->yScene_4->setValue(ui->yScene_5->value());ui->Comment_4->setText(ui->Comment_5->text());ui->Range_4->setValue(ui->Range_5->value());ui->Bearing_4->setValue(ui->Bearing_5->value());m_photoDate[3]=m_photoDate[4];m_photoFilename[3] = m_photoFilename[4];m_locFilename[3]=m_locFilename[4];}
    if (idx <=5) {ui->xScene_5->setValue(ui->xScene_6->value());ui->yScene_5->setValue(ui->yScene_6->value());ui->Comment_5->setText(ui->Comment_6->text());ui->Range_5->setValue(ui->Range_6->value());ui->Bearing_5->setValue(ui->Bearing_6->value());m_photoDate[4]=m_photoDate[5];m_photoFilename[4] = m_photoFilename[5];m_locFilename[4]=m_locFilename[5];}
    if (idx <=6){ ui->xScene_6->setValue(ui->xScene_7->value());ui->yScene_6->setValue(ui->yScene_7->value());ui->Comment_6->setText(ui->Comment_7->text());ui->Range_6->setValue(ui->Range_7->value());ui->Bearing_6->setValue(ui->Bearing_7->value());m_photoDate[5]=m_photoDate[6];m_photoFilename[5] = m_photoFilename[6];m_locFilename[5]=m_locFilename[5];}
    if (idx <=7){ ui->xScene_7->setValue(0);ui->yScene_7->setValue(0);ui->Comment_7->setText("");ui->Range_7->setValue(0);ui->Bearing_7->setValue(0);m_photoDate[5]="";m_photoFilename[5] = "";m_locFilename[5]="";}

    if (ui->Range_1->value() == 0.0)
        {m_targetCnt = 0; return;}
    if (ui->Range_2->value() == 0.0)
        {m_targetCnt = 1; return;}
    if (ui->Range_3->value() == 0.0)
        {m_targetCnt = 2; return;}
    if (ui->Range_4->value() == 0.0)
        {m_targetCnt = 3; return;}
    if (ui->Range_5->value() == 0.0)
        {m_targetCnt = 4; return;}
    if (ui->Range_6->value() == 0.0)
        {m_targetCnt = 5; return;}
    if (ui->Range_7->value() == 0.0)
        {m_targetCnt = 6; return;}
}

void Theo::loadPhoto(){
//    QString fileName = ui->photoFolder->text() + "capture_2017-08-14_17-03-35.jpg";
    QString fileName = QFileDialog::getOpenFileName(this,
               tr("Open Image"), ui->photoFolder->text(), tr("Image Files (*.png *.jpg *.JPG *.bmp)"));
    if (fileName == "")
        return;
    QStringList items = fileName.split("/");
    QString dir = ""; QString outputDir = ui->saveFolder->text();
    for (int i=0;i<items.length()-1;i++){
        dir += items[i]+"/";
        if (i == items.length()-3 && outputDir ==""){
            ui->saveFolder->setText(dir+"Theo_Output/");
        }
    }
    QDir currentDir = QDir(dir);
    m_imageFileList.clear();
    m_imageFileList = currentDir.entryList(QStringList("*"),
                                           QDir::Files | QDir::NoSymLinks);
    m_thisImageIdx = 0;
    m_thisImageIdx = m_imageFileList.indexOf(items.last());
    ui->photoFolder->setText(dir);
    ui->priorFile->setEnabled(true);
    ui->postFile->setEnabled(true);

    openImageFile(true);
}
int Theo::getExifData(QString filename){
    // Read the JPEG file into a buffer
    FILE *fp = fopen(filename.toLatin1().data(), "rb");
    if (!fp) {
      printf("Can't open file.\n");
      return -1;
    }
    fseek(fp, 0, SEEK_END);
    unsigned long fsize = ftell(fp);
    rewind(fp);
    unsigned char *buf = new unsigned char[fsize];
    if (fread(buf, 1, fsize, fp) != fsize) {
      printf("Can't read file.\n");
      delete[] buf;
      return -2;
    }
    fclose(fp);

    // Parse EXIF
    easyexif::EXIFInfo result;
    int code = result.parseFrom(buf, fsize);
    delete[] buf;
    if (code) {
      printf("Error parsing EXIF: code %d\n", code);
      return -3;
    }
    qDebug()<<result.DateTime.c_str();
    qDebug()<<QString(result.DateTime.c_str());
    ui->photoDate->setText(QString(result.DateTime.c_str()));
    int iWidth = result.ImageWidth;
    int iHeight = result.ImageHeight;
    ui->CCD_x->setValue(iWidth);
    ui->CCD_y->setValue(iHeight);
}
void Theo::openImageFile(bool rescaleToView){
    QString fileName = m_imageFileList.at(m_thisImageIdx);

    ui->photoFilename->setText(fileName);
    QStringList items = fileName.split(".");
    qDebug() <<items << items[0] <<items[1];
    if (!(items[1] == "jpg" || items[1] == "JPG")){
        return;
    }
    if (ui->chkTideHt->isChecked()){
        items = fileName.split("_");
        ui->photoDate->setText(items[1]);
        if (items.length()<4){
            QMessageBox msgBox;
            msgBox.setText("The tide height does not seem to be in the file name. Run tideCalcs.py");
            msgBox.exec();
            return;
        }
        items = items[3].split(".");
        items = items[0].split("-");
        double htcam = items[0].toInt()+items[1].toDouble()/60.0;
        ui->zCamera->setValue(htcam);
    }
    /////////////////////////   get EXIF data from image
    int status = getExifData(ui->photoFolder->text()+fileName);

    ///////////////////////
    /// \brief loadTheImage
    QString fullPathFile = ui->photoFolder->text()+fileName;
    loadTheImage(fullPathFile, rescaleToView, false);  // false signifies not ready to draw contour lines yet

    m_keyPressed = 0;
}
void Theo::loadTheImage(QString theFile, bool rescaleToView, bool drawContours){
    m_img.load(theFile);
//    QImage scaledImg = img.scaled(ui->graphicsView->size(),
//                           Qt::KeepAspectRatio,
//                           Qt::SmoothTransformation);
//    g_pixmap = QPixmap::fromImage(scaledImg);

    g_pixmap = QPixmap::fromImage(m_img);   // set pixmap to full scale of image
    if (drawContours){
//        for (int i=0;i<500;i++)                 // this draws a black line on the image.
//            m_img.setPixel(i,i, qRgb(0, 0, 0));

        QPainter painter;
        painter.begin(&g_pixmap);
        QPen penx(Qt::red, 5 );
        painter.setPen(penx);
        for (int k=0;k<5;k++){
            int x1 = m_utmXline_px_x[0][k]; int y1 = m_utmXline_px_y[0][k];
            int x2 = m_utmXline_px_x[1][k]; int y2 = m_utmXline_px_y[1][k];


            painter.drawLine(x1,y1,x2,y2);
        }
        QPen peny(Qt::green, 5 );
        painter.setPen(peny);
        for (int k=0;k<5;k++){
            int x1 = m_utmYline_px_x[0][k]; int y1 = m_utmYline_px_y[0][k];
            int x2 = m_utmYline_px_x[1][k]; int y2 = m_utmYline_px_y[1][k];
            painter.drawLine(x1,y1,x2,y2);
        }
        painter.end();
    }
    qDebug()<<"g_pixmap h,w"<<g_pixmap.height()<<g_pixmap.width();
    ui->graphicsView->scene()->addPixmap(g_pixmap);
    if (rescaleToView){
        double factor = (double)ui->graphicsView->viewport()->height()/m_img.height();
        qDebug()<<"REscale----graphicsView height"<<ui->graphicsView->viewport()->height()<<"image height"<<m_img.height()<<"ratio"<<factor;
        if (m_firstImage)
            ui->graphicsView->scale(factor,factor);
        m_firstImage = false;
        QPointF screenCenter = QPointF(ui->graphicsView->viewport()->width()/2.0, ui->graphicsView->viewport()->height()/2.0);
        ui->graphicsView->centerOn(ui->graphicsView->mapToScene(screenCenter.toPoint()));
    }
    ui->graphicsView->viewport()->update();
    qDebug()<<"graphicsView h,w"<<ui->graphicsView->viewport()->height()<<ui->graphicsView->viewport()->width();

}
void Theo::advanceOneImage(){
    if (m_thisImageIdx < m_imageFileList.length()-1)
        m_thisImageIdx++;
    else
        m_thisImageIdx = 0;
    openImageFile(false);
}
void Theo::retreatOneImage(){
    if (m_thisImageIdx > 0)
        m_thisImageIdx--;
    else
        m_thisImageIdx = m_imageFileList.length() - 1;
    openImageFile(false);
}

void Theo::keyPressEvent(QKeyEvent * event)
{
    if (event->isAutoRepeat())
        return;
    //qDebug()<<"here is the number of the key that was pressed "<<event->key();
    m_keyPressed = event->key();
}
void Theo::keyReleaseEvent(QKeyEvent *event){
   if (!event->isAutoRepeat()){
      // qDebug()<<"key released";
     m_keyPressed = 0;
   }
}

void Theo::gotMouseSelection(QList<double> coords){
    qDebug()<<"got this point"<<coords<<"with this keypressed"<<m_keyPressed;
    processClick(m_keyPressed,coords[0],coords[1]);
}

void Theo::processClick(int key, double x, double y){
    //qDebug()<<"processClick with "<<key<<x<<y;
    // '1' = 49, '2' = 50, '3' = 51
    if (key == 49){  // this is the first fiducial point
        PIXxFiducial[0] = x;
        PIXyFiducial[0] = y;
        double range;
        range = sqrt(SQR(m_xyH[0][0] - m_xyH[0][1]) + SQR(m_xyH[1][0] - m_xyH[1][1]));
        double bearing = atan2(m_xyH[0][1] - m_xyH[0][0],  m_xyH[1][1] - m_xyH[1][0])*RAD2DEG;
        if (bearing < 0) bearing += 360;
        qDebug()<<"first fiducial is at"<<x<<y<<"range"<<range<<"bearing"<<bearing<<m_xyH[0][1] - m_xyH[0][0]<<m_xyH[1][1] - m_xyH[1][0];
//        calculateCameraBearingAz();
    }

    if (key == 50){  // this is the second fiducial point
        PIXxFiducial[1] = x;
        PIXyFiducial[1] = y;
        double range;
        range = sqrt(SQR(m_xyH[0][0] - m_xyH[0][2]) + SQR(m_xyH[1][0] - m_xyH[1][2]));
        double bearing = atan2(m_xyH[0][2] - m_xyH[0][0],  m_xyH[1][2] - m_xyH[1][0])*RAD2DEG;
        if (bearing < 0) bearing += 360;
        qDebug()<<"second fiducial is at"<<x<<y<<"range"<<range<<"bearing"<<bearing;
//        calculateCameraBearingAz();
    }

    if (key == 51){  // this is the third fiducial point
        PIXxFiducial[2] = x;
        PIXyFiducial[2] = y; 
        double range = sqrt(SQR(m_xyH[0][0] - m_xyH[0][3]) + SQR(m_xyH[1][0] - m_xyH[1][3]));
        double bearing = atan2(m_xyH[0][3] - m_xyH[0][0],  m_xyH[1][3] - m_xyH[1][0])*RAD2DEG;
        if (bearing < 0) bearing += 360;
        qDebug()<<"third fiducial is at"<<x<<y<<"range"<<range<<"bearing"<<bearing;
//        calculateCameraBearingAz();
    }
    if (key == 52){  // this is the fourth fiducial point
        PIXxFiducial[3] = x;
        PIXyFiducial[3] = y;
        double range = sqrt(SQR(m_xyH[0][0] - m_xyH[0][4]) + SQR(m_xyH[1][0] - m_xyH[1][4]));
        double bearing = atan2(m_xyH[0][4] - m_xyH[0][0],  m_xyH[1][4] - m_xyH[1][0])*RAD2DEG;
        if (bearing < 0) bearing += 360;
        qDebug()<<"fourth fiducial is at"<<x<<y<<"range"<<range<<"bearing"<<bearing;
        m_needDigitizeFids = false;
        precomputeSpace();
    }

    //  'CTRL' = 16777249
    if (key == 16777249){
        if (!m_needPrecompute && !m_needDigitizeFids){
 //           calculateUTMsFlat(x,y);
            lookupUTMs(x,y);
        }
        else {
            QMessageBox msgBox;
            msgBox.setText("You have to number-click on the 4 fiducials with 4th one last!");  // Note Bene -- need to generalize this test
            msgBox.exec();
            m_needPrecompute = true;
        }
        m_graphicsViewWidth = ui->graphicsView->width();
    }
}
double Theo::getBearingFromLatLong(int i){
    double K;  //  [0][x] is latitude  [1][x] is longitude [x][0] is camera, [x][1] is first fiductial etc.
    K = atan2(sin(m_LatLongH[1][i] - m_LatLongH[1][0])*cos(m_LatLongH[0][i]), cos(m_LatLongH[0][0])*sin(m_LatLongH[0][i]) -
            sin(m_LatLongH[0][0])*cos(m_LatLongH[0][i])*cos(m_LatLongH[1][i] - m_LatLongH[1][0]));
    if (K<0) K += 2*3.1416;
    while (K>2*3.1416)
        K -= 2*3.1416;
    qDebug()<<"bearing of fiducial"<<i<<"from lat/long"<<K*RAD2DEG;
    return K;
}

double Theo::calcEpsilon(VecDoub R0, double FOVy, double CCDy){  // elevation angle of camera
    // use PIXyFiducial for fiducial pixels and m_xyH for range values
    double epsilon = 0;
    VecDoub eps;
    eps.resize(4);
    double epstot = 0;
    for (int i=0; i<=3; i++){
        double theta_a;  // dip angle (radians) from lens axis to a fiducial or target
        theta_a = 0.5*(FOVy*DEG2RAD)*(0.5 - PIXyFiducial[i]/CCDy);
        double theta_A;  // elevation angle from fiducial or target to camera
        theta_A = atan2(m_xyH[2][0] - m_xyH[2][i+1], R0[i]);
        if (theta_A < 0){
            theta_A += 2*3.1416;
        }
        eps[i] = theta_A+theta_a;
        epstot += eps[i];
        qDebug()<<"i is"<<i<<"eps is"<<eps[i]*RAD2DEG<<"theta_A"<<theta_A*RAD2DEG<<"theta_a"<<theta_a*RAD2DEG;
        qDebug()<<"...";
    }
    epsilon = epstot / 4.0;   // average of the four elevation values
    epsilon = (eps[0]+eps[1]+eps[2])/3.0;   // N. B. this omits the contribution by the close fiducial N. B. Note Bene
    return epsilon;
}


double Theo::calcCameraAz(double FOVx, double CCDx){
    double phi = 0;
    VecDoub phis;
    phis.resize(4);
    double phitot = 0;
    for (int i=0;i<=3; i++){
        double phi_b;  // angle (from above) from fid or target to lens axis
        phi_b = (FOVx*DEG2RAD)*(PIXxFiducial[i]/(float)CCDx - 0.5);
        qDebug()<<"phi fid"<<i<<"to camera axis"<<phi_b*RAD2DEG<<FOVx*DEG2RAD<<0.5*CCDx<<PIXxFiducial[i];
        double phi_B;  // Azimuthal angle to point

        phi_B = atan2(m_xyH[0][i+1] - m_xyH[0][0],  m_xyH[1][i+1] - m_xyH[1][0]);
        if (phi_B < 0){
            phi_B += 2*3.1416;
        }
        phis[i] = phi_B - phi_b;
        phitot += phis[i];
        qDebug()<<"   deltay"<<m_xyH[1][0] << m_xyH[1][i+1]<<-(m_xyH[1][0] - m_xyH[1][i+1]);
        qDebug()<<"   deltax"<<m_xyH[0][0] << m_xyH[0][i+1]<<-(m_xyH[0][0] - m_xyH[0][i+1]);
        qDebug()<<"   phi_B"<< phi_B*RAD2DEG;

        qDebug()<<"   phis[i] is"<<phis[i]*RAD2DEG<<"at ccd: phi_b"<<phi_b*RAD2DEG;
    }
    phi = (phis[0]+phis[1]+phis[2])/3.0;
    qDebug()<<"     Camera Axis azimuth ="<<phi*RAD2DEG;
    return phi;
}
void Theo::getRangeAndBearingDeltaXY(double x, double y, double &range, double &bearing, double &xTarget, double &yTarget){

    double theta_Target = ui->tiltCam->value()*DEG2RAD + 0.5*(y-ui->CCD_y->value()*m_radPerPixel);


    qDebug()<<"theta target="<<theta_Target*RAD2DEG;
    range = m_xyH[2][0]/tan(theta_Target);

    double phiCam = ui->AzCam->value()*DEG2RAD;

    double phi_a;  // azimuthal angle from lens axis to target
    phi_a = (ui->fov_X->value()*DEG2RAD)*(x/(float)ui->CCD_x->value() - 0.5);
    bearing = phiCam + phi_a;
    xTarget = m_xyH[0][0] + range*sin(bearing);
    yTarget = m_xyH[1][0] + range*cos(bearing);
    qDebug()<<"x/(float)CCDx"<<x/(float)ui->CCD_x->value();
    qDebug()<<"x="<<x<<"y="<<y<<"range="<<range<<"bearing="<<bearing*RAD2DEG<<"phi_from_camera_axis"<<phi_a*RAD2DEG;
    qDebug()<<"xTarget="<<xTarget<<"yTarget="<<yTarget;


}

void Theo::lookupUTMs(int i, int j){
    if ( (i<0) || (j<0) || (i>ui->CCD_x->value()) || (j >= ui->CCD_y->value()))
        return;
    if (m_needDigitizeFids){

    }
    if (m_needPrecompute){
        precomputeSpace();
        m_needPrecompute = false;
    }
    double xTarget = m_UTMx_Img[i][j];
    double yTarget = m_UTMy_Img[i][j];
    double dx = xTarget - ui->xCamera->value();
    double dy = yTarget - ui->yCamera->value();

    double range = sqrt(SQR(dx)+SQR(dy));
    double bearing = atan2(dx,dy);
    if (bearing < 0)
        bearing += 2*3.1416;
    m_photoDate[m_targetCnt] = ui->photoDate->text();
    m_photoFilename[m_targetCnt] = ui->photoFilename->text();
    m_locFilename[m_targetCnt] = ui->locFilename->text();
    if (m_targetCnt == 0){
        ui->Range_1->setValue(range);
        ui->xScene_1->setValue(xTarget);
        ui->Bearing_1->setValue(bearing*RAD2DEG);
        ui->yScene_1->setValue(yTarget);
        ui->Comment_1->setText("");

    }
    if (m_targetCnt == 1){
        ui->Range_2->setValue(range);
        ui->xScene_2->setValue(xTarget);
        ui->yScene_2->setValue(yTarget);
        ui->Bearing_2->setValue(bearing*RAD2DEG);
        ui->Comment_2->setText("");
    }
    if (m_targetCnt == 2){
        ui->Range_3->setValue(range);
        ui->xScene_3->setValue(xTarget);
        ui->yScene_3->setValue(yTarget);
        ui->Bearing_3->setValue(bearing*RAD2DEG);
        ui->Comment_3->setText("");
    }

    if (m_targetCnt == 3){
        ui->Range_4->setValue(range);
        ui->xScene_4->setValue(xTarget);
        ui->yScene_4->setValue(yTarget);
        ui->Bearing_4->setValue(bearing*RAD2DEG);
        ui->Comment_4->setText("");
    }
    if (m_targetCnt == 4){
        ui->Range_5->setValue(range);
        ui->xScene_5->setValue(xTarget);
        ui->yScene_5->setValue(yTarget);
        ui->Bearing_5->setValue(bearing*RAD2DEG);
        ui->Comment_5->setText("");
    }
    if (m_targetCnt == 5){
        ui->Range_6->setValue(range);
        ui->xScene_6->setValue(xTarget);
        ui->yScene_6->setValue(yTarget);
        ui->Bearing_6->setValue(bearing*RAD2DEG);
        ui->Comment_6->setText("");
    }
    if (m_targetCnt == 6){
        ui->Range_7->setValue(range);
        ui->xScene_7->setValue(xTarget);
        ui->yScene_7->setValue(yTarget);
        ui->Bearing_7->setValue(bearing*RAD2DEG);
        ui->Comment_7->setText("");
    }
    m_targetCnt++;
    if (m_targetCnt == 7){
        m_targetCnt = 0;
        // Note Bene  could automatically save all targets if user askes for more than 5
    }
}

void Theo::calculateUTMsFlat(double x, double y){
    // x conversion: 480347 = -123.26642 and long decreases as x increases
    // y conversion  5379952 = 48.57235

    // goal -- take the x,y pair in pixels and calculate location in Lat/Long and UTMs

    double range; double bearing; double xTarget; double yTarget;
    getRangeAndBearingDeltaXY(x,y, range, bearing, xTarget, yTarget);

    qDebug()<<"---------------------------------------range"<<range;
    qDebug()<<"---------------------------bearing of target"<<bearing*RAD2DEG;
    qDebug()<< "UTMxTarget"<<xTarget<<"UTMx fid"<< m_xyH[0][0]<<"deltaX (m)" << range*sin(bearing);
    qDebug() << "UTMyTarget"<<QString::number(yTarget,'f',4)<<"UTMy fid"<< QString::number(m_xyH[1][0],'f',4) <<"deltaY (m)"<< range*cos(bearing);

    m_photoDate[m_targetCnt] = ui->photoDate->text();
    m_photoFilename[m_targetCnt] = ui->photoFilename->text();
    m_locFilename[m_targetCnt] = ui->locFilename->text();
    if (m_targetCnt == 0){
        ui->Range_1->setValue(range);
        ui->xScene_1->setValue(xTarget);
        ui->Bearing_1->setValue(bearing*RAD2DEG);
        ui->yScene_1->setValue(yTarget);
        ui->Comment_1->setText("");

    }
    if (m_targetCnt == 1){
        ui->Range_2->setValue(range);
        ui->xScene_2->setValue(xTarget);
        ui->yScene_2->setValue(yTarget);
        ui->Bearing_2->setValue(bearing*RAD2DEG);
        ui->Comment_2->setText("");
    }
    if (m_targetCnt == 2){
        ui->Range_3->setValue(range);
        ui->xScene_3->setValue(xTarget);
        ui->yScene_3->setValue(yTarget);
        ui->Bearing_3->setValue(bearing*RAD2DEG);
        ui->Comment_3->setText("");
    }

    if (m_targetCnt == 3){
        ui->Range_4->setValue(range);
        ui->xScene_4->setValue(xTarget);
        ui->yScene_4->setValue(yTarget);
        ui->Bearing_4->setValue(bearing*RAD2DEG);
        ui->Comment_4->setText("");
    }
    if (m_targetCnt == 4){
        ui->Range_5->setValue(range);
        ui->xScene_5->setValue(xTarget);
        ui->yScene_5->setValue(yTarget);
        ui->Bearing_5->setValue(bearing*RAD2DEG);
        ui->Comment_5->setText("");
    }
    if (m_targetCnt == 5){
        ui->Range_6->setValue(range);
        ui->xScene_6->setValue(xTarget);
        ui->yScene_6->setValue(yTarget);
        ui->Bearing_6->setValue(bearing*RAD2DEG);
        ui->Comment_6->setText("");
    }

    m_targetCnt++;
    if (m_targetCnt == 6){
        m_targetCnt = 0;
        // Note Bene  could automatically save all targets if user askes for more than 5
    }



}
