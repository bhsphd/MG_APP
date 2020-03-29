#include "QPPPBackSmooth.h"

QStringList QPPPBackSmooth::searchFilterFile(QString floder_path, QStringList filers)
{
    QDir floder_Dir(floder_path);
    QStringList all_file_path;
    floder_Dir.setFilter(QDir::Files | QDir::NoSymLinks);
    floder_Dir.setNameFilters(filers);
    QFileInfoList file_infos = floder_Dir.entryInfoList();
    for(int i = 0;i < file_infos.length();i++)
    {
        QFileInfo file_info = file_infos.at(i);
        if(file_info.fileName() != "." || file_info.fileName() != "..")
            all_file_path.append(file_info.absoluteFilePath());
    }
    return all_file_path;
}

//Run the specified directory file
QPPPBackSmooth::QPPPBackSmooth(QString files_path,  QTextEdit *pQTextEdit, QString Method, QString Satsystem,
                     QString TropDelay, double CutAngle, bool isKinematic, QString Smooth_Str)
{
    // Display for GUI
    mp_QTextEditforDisplay = pQTextEdit;
    //Initialize variables
    initVar();
    m_run_floder = files_path + PATHSEG;
    m_App_floder = QCoreApplication::applicationDirPath() + PATHSEG;
    // find files
    QStringList tempFilters,
            OFileNamesList, Sp3FileNamesList, ClkFileNamesList, ErpFileNamesList,
            AtxFileNamesList, BlqFileNamesList, GrdFileNamesList;
    // find obs files
    tempFilters.clear();
    tempFilters.append("*.*o");
    OFileNamesList = searchFilterFile(m_run_floder, tempFilters);
    // find sp3 files
    tempFilters.clear();
    tempFilters.append("*.sp3");
    Sp3FileNamesList = searchFilterFile(m_run_floder, tempFilters);
    // find clk files
    tempFilters.clear();
    tempFilters.append("*.clk");
    ClkFileNamesList = searchFilterFile(m_run_floder, tempFilters);
    // find erp files
    tempFilters.clear();
    tempFilters.append("*.erp");
    ErpFileNamesList = searchFilterFile(m_run_floder, tempFilters);
    // find Atx files
    tempFilters.clear();
    tempFilters.append("*.atx");
    AtxFileNamesList = searchFilterFile(m_App_floder, tempFilters);
    // find blq files
    tempFilters.clear();
    tempFilters.append("*.blq");
    BlqFileNamesList = searchFilterFile(m_App_floder, tempFilters);
    // find grd files
    tempFilters.clear();
    tempFilters.append("*.grd");
    GrdFileNamesList = searchFilterFile(m_App_floder, tempFilters);
    // get want file
    // o file
    QString OfileName = "", erpFile = "", blqFile = "", atxFile = "", grdFile = "";
    if (!OFileNamesList.isEmpty())
    {
        OfileName = OFileNamesList.at(0);
        m_haveObsFile = true;
    }
    else
    {
        ErroTrace("QPPPBackSmooth::QPPPBackSmooth: Cant not find Obsvertion file.");
        m_haveObsFile = false;
    }
    if(!ErpFileNamesList.isEmpty()) erpFile = ErpFileNamesList.at(0);
    if(!AtxFileNamesList.isEmpty()) atxFile = AtxFileNamesList.at(0);
    if(!BlqFileNamesList.isEmpty()) blqFile = BlqFileNamesList.at(0);
    if(!GrdFileNamesList.isEmpty()) grdFile = GrdFileNamesList.at(0);

    // use defualt config
    setConfigure(Method, Satsystem, TropDelay, CutAngle, isKinematic, Smooth_Str);
    // save data to QPPPBackSmooth
    initQPPPBackSmooth(OfileName, Sp3FileNamesList, ClkFileNamesList, erpFile, blqFile, atxFile, grdFile);
}
void QPPPBackSmooth::setConfigure(QString Method, QString Satsystem, QString TropDelay, double CutAngle, bool isKinematic, QString Smooth_Str)
{
    // Configure
    m_Solver_Method = Method;// m_Solver_Method value can be "SRIF" or "Kalman"
    m_CutAngle = CutAngle;// (degree)
    m_SatSystem = Satsystem;// GPS, GLONASS, BDS, and Galieo are used respectively: the letters G, R, C, E
    m_TropDelay = TropDelay;// The tropospheric model m_TropDelay can choose Sass, Hopfiled, UNB3m
    m_Smooth_Str = Smooth_Str;

    //Setting up the file system. SystemStr:"G"(Turn on the GPS system);"GR":(Turn on the GPS+GLONASS system);"GRCE"(Open all)et al
    //GPS, GLONASS, BDS, and Galieo are used respectively: the letters G, R, C, E
    setSatlitSys(Satsystem);
    m_sys_str = Satsystem;
    m_sys_num = getSystemnum();

   if(isKinematic)
   {
       m_KalmanClass.setModel(QKalmanFilter::KALMAN_MODEL::PPP_KINEMATIC);// set Kinematic model
       m_SRIFAlgorithm.setModel(SRIFAlgorithm::SRIF_MODEL::PPP_KINEMATIC);
   }
   else
   {
       m_KalmanClass.setModel(QKalmanFilter::KALMAN_MODEL::PPP_STATIC);// set static model
       m_SRIFAlgorithm.setModel(SRIFAlgorithm::SRIF_MODEL::PPP_STATIC);
       m_minSatFlag = 1;// Dynamic Settings 5, Static Settings 1 in setConfigure()
   }


   if("Smooth" == Smooth_Str)
   {
       m_KalmanClass.setSmoothRange(QKalmanFilter::KALMAN_SMOOTH_RANGE::SMOOTH);
       m_SRIFAlgorithm.setSmoothRange(SRIFAlgorithm::SRIF_SMOOTH_RANGE::SMOOTH);
       m_isSmoothRange = true;// Whether to use phase smoothing pseudorange for SPP
   }
   else if("NoSmooth" == Smooth_Str)
   {
       m_KalmanClass.setSmoothRange(QKalmanFilter::KALMAN_SMOOTH_RANGE::NO_SMOOTH);
       m_SRIFAlgorithm.setSmoothRange(SRIFAlgorithm::SRIF_SMOOTH_RANGE::NO_SMOOTH);
       m_isSmoothRange = false;// Whether to use phase smoothing pseudorange for SPP
   }

   m_isKinematic = isKinematic;
}

//Initialization operation
void QPPPBackSmooth::initVar()
{
    for (int i = 0;i < 3;i++)
        m_ApproxRecPos[0] = 0;
    m_OFileName = "";
    multReadOFile = 99999999999;// read all epoch in obeservation data
    m_leapSeconds = 0;
    m_isConnect = false;
    m_run_floder = "";
    m_haveObsFile = false;
    m_isRuned = false;
    m_isInitSPP = false;
    m_save_images_path = "";
    m_iswritre_file = false;
    m_minSatFlag = 4;// Dynamic Settings 5, Static Settings 1 in setConfigure()
    m_isSmoothRange = false;// Whether to use phase smoothing pseudorange for SPP
}

//Constructor
void QPPPBackSmooth::initQPPPBackSmooth(QString OFileName,QStringList Sp3FileNames,QStringList ClkFileNames,QString ErpFileName,QString BlqFileName,QString AtxFileName,QString GrdFileName)
{
    if(!m_haveObsFile) return ;// if not have observation file.
//Set up multi-system data
//Initial various classes
    m_ReadSP3Class.setSP3FileNames(Sp3FileNames);
    m_ReadClkClass.setClkFileNames(ClkFileNames);
    m_ReadOFileClass.setObsFileName(OFileName);
    m_ReadTropClass.setTropFileNames(GrdFileName,"GMF", m_TropDelay);// Default tropospheric model projection function GMF
    m_ReadAntClass.setAntFileName(AtxFileName);
    m_TideEffectClass.setTideFileName(BlqFileName,ErpFileName);

//Save file name
    m_OFileName = OFileName;
    m_Sp3FileNames = Sp3FileNames;
    m_ClkFileNames = ClkFileNames;
//Various class settings
    int obsTime[5] = {0};
    double Seconds = 0,ObsJD = 0;
    m_ReadOFileClass.getApproXYZ(m_ApproxRecPos);//Obtain the approximate coordinates of the O file
    m_ReadOFileClass.getFistObsTime(obsTime,Seconds);//Get the initial observation time
    ObsJD = qCmpGpsT.computeJD(obsTime[0],obsTime[1],obsTime[2],obsTime[3],obsTime[4],Seconds);
    m_ReadAntClass.setObsJD(m_ReadOFileClass.getAntType(),ObsJD);//Set the antenna effective time
    m_TideEffectClass.setStationName(m_ReadOFileClass.getMakerName());//Setting the tide requires a station name
//Search products and download
    int GPS_Week = 0, GPS_Day = 0;
    qCmpGpsT.YMD2GPSTime(obsTime[0],obsTime[1],obsTime[2],obsTime[3],obsTime[4],Seconds, &GPS_Week, &GPS_Day);
    //If there is no product, it is necessary to make up such products.
    if(Sp3FileNames.isEmpty()|| ClkFileNames.isEmpty())
    {
        connectHost();
    }
    if(Sp3FileNames.isEmpty())
    {
        m_Sp3FileNames = downProducts(m_run_floder, GPS_Week, GPS_Day, "sp3");
        m_ReadSP3Class.setSP3FileNames(m_Sp3FileNames);
    }
    if(ClkFileNames.isEmpty())
    {
        m_ClkFileNames = downProducts(m_run_floder, GPS_Week, GPS_Day, "clk");
        m_ReadClkClass.setClkFileNames(m_ClkFileNames);
    }

//Get skip seconds
    m_leapSeconds = qCmpGpsT.getLeapSecond(obsTime[0],obsTime[1],obsTime[2],obsTime[3],obsTime[4],Seconds);

//Read the required calculation file module (time consuming)
    m_ReadAntClass.getAllData();//Read all data from satellites and receivers
    m_TideEffectClass.getAllData();//Read tide data
    m_ReadTropClass.getAllData();//Read grd files for later tropospheric calculations
    m_ReadSP3Class.getAllData();//Read the entire SP3 file
    m_ReadClkClass.getAllData();//Read the clock error file for later calculation
}

// get matrix B and observer L(QPPPBackSmooth::Obtaining_equation() is same function as QPPPModel::Obtaining_equation() )
void QPPPBackSmooth::Obtaining_equation(QVector< SatlitData > &currEpoch, double *ApproxRecPos, MatrixXd &mat_B, VectorXd &Vct_L, MatrixXd &mat_P, bool isSmoothRange)
{
    int epochLenLB = currEpoch.length();
    MatrixXd B, P;
    VectorXd L, sys_len;

    sys_len.resize(m_sys_str.length());
    B.resize(epochLenLB,3 + m_sys_num);
    P.resize(epochLenLB,epochLenLB);
    L.resize(epochLenLB);

    sys_len.setZero();
    B.setZero();
    L.setZero();
    P.setIdentity();
    bool is_find_base_sat = false;

    for (int i = 0; i < epochLenLB;i++)
    {
        SatlitData oneSatlit = currEpoch.at(i);
        double li = 0,mi = 0,ni = 0,p0 = 0,dltaX = 0,dltaY = 0,dltaZ = 0;
        dltaX = oneSatlit.X - ApproxRecPos[0];
        dltaY = oneSatlit.Y - ApproxRecPos[1];
        dltaZ = oneSatlit.Z - ApproxRecPos[2];
        p0 = qSqrt(dltaX*dltaX+dltaY*dltaY+dltaZ*dltaZ);
        li = dltaX/p0;mi = dltaY/p0;ni = dltaZ/p0;
        //Computational B matrix
        //P3 pseudorange code matrix
        B(i, 0) = li;B(i, 1) = mi;B(i, 2) = ni; B(i, 3) = -1;// base system satlite clk must exist
        // debug by xiaogongwei 2019.04.03 for ISB
        for(int k = 1; k < m_sys_str.length();k++)
        {
            if(m_sys_str[k] == oneSatlit.SatType)
            {
                B(i,3+k) = -1;
                sys_len[k] = 1;//good no zeros cloumn in B,sys_lenmybe 0 1 1 0
            }
        }
        // is exist base system satlite clk
        if(m_sys_str[0] == oneSatlit.SatType)
            is_find_base_sat = true;

        //Calculating the L matrix
        double dlta = 0;//Correction of each
        dlta =  - oneSatlit.StaClock + oneSatlit.SatTrop - oneSatlit.Relativty -
            oneSatlit.Sagnac - oneSatlit.TideEffect - oneSatlit.AntHeight;
        //Pseudorange code PP3
        if(isSmoothRange)
        {// add by xiaogongwei 2018.11.20
            L(i) = p0 - oneSatlit.PP3_Smooth + dlta;
            // Computing weight matrix P
//            if(oneSatlit.UTCTime.epochNum > 30 && oneSatlit.PP3_Smooth_NUM < 30 )//
//                P(i, i) = 0.0001;
//            else
                P(i, i) = 1 / oneSatlit.PP3_Smooth_Q;//Smooth pseudorange????
        }
        else
        {
            L(i) = p0 - oneSatlit.PP3 + dlta;
            // Computing weight matrix P
            P(i, i) = oneSatlit.SatWight;//Pseudo-range right
        }

    }//B, L is calculated
    // save data to mat_B
    mat_B = B;
    Vct_L = L;
    mat_P = P;
    // debug by xiaogongwei 2019.04.04
    int no_zero = sys_len.size() - 1 - sys_len.sum();
    if(no_zero > 0 || !is_find_base_sat)
    {
        int new_hang = B.rows() + no_zero, new_lie = B.cols(), flag = 0;
        if(!is_find_base_sat) new_hang++; // check base system satlite clk is exist
        mat_B.resize(new_hang,new_lie);
        mat_P.resize(new_hang,new_hang);
        Vct_L.resize(new_hang);
        mat_B.setZero();
        Vct_L.setZero();
        mat_P.setIdentity();
        // check base system satlite clk is exist
        if(!is_find_base_sat)
        {
            for(int i = 0;i < B.rows();i++)
                B(i, 3) = 0;
            mat_B(mat_B.rows() - 1, 3) = 1;
        }
        mat_B.block(0,0,B.rows(),B.cols()) = B;
        mat_P.block(0,0,P.rows(),P.cols()) = P;
        Vct_L.head(L.rows()) = L;
        for(int i = 1; i < sys_len.size();i++)
        {
            if(0 == sys_len[i])
            {
                mat_B(epochLenLB+flag, 3+i) = 1;// 3 is conntain [dx,dy,dz,mf]
                flag++;
            }

        }
    }//if(no_zero > 0)
}

void QPPPBackSmooth::SimpleSPP(QVector < SatlitData > &prevEpochSatlitData, QVector < SatlitData > &epochSatlitData, double *spp_pos)
{
    double p_HEN[3] = {0};
    m_ReadOFileClass.getAntHEN(p_HEN);//Get the antenna high
    GPSPosTime epochTime = epochSatlitData.at(0).UTCTime;//Obtaining observation time

    Vector3d tempPos3d, diff_3d;
    tempPos3d[0] = spp_pos[0]; tempPos3d[1] = spp_pos[1]; tempPos3d[2] = spp_pos[2];
    QVector< SatlitData > store_currEpoch;
    int max_iter = 20;
    for(int iterj = 0; iterj < max_iter; iterj++)
    {
        QVector< SatlitData > currEpoch;
        // get every satilite data
        for (int i = 0;i < epochSatlitData.length();i++)
        {
            SatlitData tempSatlitData = epochSatlitData.at(i);//Store calculated corrected satellite data
//Test whether the carrier and pseudorange are abnormal and terminate in time.
            if(!(tempSatlitData.L1&&tempSatlitData.L2&&tempSatlitData.C1&&tempSatlitData.C2))
                continue;
            //When seeking GPS
            double m_PrnGpst = qCmpGpsT.YMD2GPSTime(epochTime.Year,epochTime.Month,epochTime.Day,
                epochTime.Hours,epochTime.Minutes,epochTime.Seconds);
            // Read satellite clock error from CLK file
            double stalitClock = 0;//Unit m
            // Note: Time is the signal transmission time(m_PrnGpst - tempSatlitData.C2/M_C)
            getCLKData(tempSatlitData.PRN,tempSatlitData.SatType,m_PrnGpst - tempSatlitData.C2/M_C,&stalitClock);
            tempSatlitData.StaClock = stalitClock;
            //Obtain the coordinates of the epoch satellite from the SP3 data data
            double pXYZ[3] = {0},pdXYZ[3] = {0};//Unit m
            // Note: Time is the signal transmission time(m_PrnGpst - tempSatlitData.C2/M_C - tempSatlitData.StaClock/M_C)
            getSP3Pos(m_PrnGpst - tempSatlitData.C2/M_C - tempSatlitData.StaClock/M_C,tempSatlitData.PRN,
                      tempSatlitData.SatType,pXYZ,pdXYZ);//Obtain the precise ephemeris coordinates of the satellite launch time(Here we need to subtract the satellite clock error tempSatlitData.StaClock/M_C Otherwise it will cause a convergence gap of 20cm)
            tempSatlitData.X = pXYZ[0];tempSatlitData.Y = pXYZ[1];tempSatlitData.Z = pXYZ[2];
            //Calculate the satellite's high sitting angle (as the receiver approximates the target)
            double EA[2]={0};
            getSatEA(tempSatlitData.X,tempSatlitData.Y,tempSatlitData.Z,spp_pos,EA);
            tempSatlitData.EA[0] = EA[0];tempSatlitData.EA[1] = EA[1];
            EA[0] = EA[0]*MM_PI/180;EA[1] = EA[1]*MM_PI/180;//Go to the arc to facilitate the calculation below
            tempSatlitData.SatWight = 0.01;// debug xiaogongwei 2018.11.16
            if( spp_pos[0] !=0 ) getWight(tempSatlitData);//Set the weight of the satellite  debug by xiaogongwei 2019.04.24
//Test the state of the precise ephemeris and the clock difference and whether the carrier and pseudorange are abnormal, and terminate the XYZ or the satellite with the clock difference of 0 in time.
            if (!(tempSatlitData.X&&tempSatlitData.Y&&tempSatlitData.Z&&tempSatlitData.StaClock))
                continue;
//Quality control (height angle , pseudorange difference)
            if (qAbs(tempSatlitData.C1 - tempSatlitData.C2) > 50)
                continue;
            if(spp_pos[0] !=0 && tempSatlitData.EA[0] < m_CutAngle)
                continue;
//At the time of SPP, the five satellites with poor GEO orbit in front of Beidou are not removed.
//          if(tempSatlitData.SatType == 'C' && tempSatlitData.PRN <=5 )
//              continue;
//Calculate the wavelength (mainly for multiple systems)
            double F1 = tempSatlitData.Frq[0],F2 = tempSatlitData.Frq[1];
            if(F1 == 0 || F2 == 0) continue;//Frequency cannot be 0
            //Computational relativity correction
            double relative = 0;
            if(spp_pos[0] !=0 ) relative = getRelativty(pXYZ,spp_pos,pdXYZ);
            tempSatlitData.Relativty = relative;
            //Calculate the autobiographic correction of the earth
            double earthW = 0;
            earthW = getSagnac(tempSatlitData.X,tempSatlitData.Y,tempSatlitData.Z,spp_pos);
            tempSatlitData.Sagnac = 0;
            if(spp_pos[0] !=0 ) tempSatlitData.Sagnac = earthW;
            //Calculate tropospheric dry delay!!!
            double MJD = qCmpGpsT.computeJD(epochTime.Year,epochTime.Month,epochTime.Day,
                epochTime.Hours,epochTime.Minutes,epochTime.Seconds) - 2400000.5;//Simplified Julian Day
            //Calculate and save the annual accumulation date
            double TDay = qCmpGpsT.YearAccDay(epochTime.Year,epochTime.Month,epochTime.Day);
            double p_BLH[3] = {0},mf = 0, TropZPD = 0;;
            qCmpGpsT.XYZ2BLH(spp_pos[0], spp_pos[1], spp_pos[2], p_BLH);
            if(spp_pos[0] !=0 ) getTropDelay(MJD,TDay,EA[0],p_BLH,&mf, NULL, &TropZPD);
            tempSatlitData.SatTrop = TropZPD;
            tempSatlitData.StaTropMap = mf;
            if(spp_pos[0] !=0 ) tempSatlitData.StaTropMap = mf;
            //Calculate antenna high offset correction  Antenna Height
            tempSatlitData.AntHeight = 0;
            if( spp_pos[0] !=0 )
                tempSatlitData.AntHeight = p_HEN[0]*qSin(EA[0]) + p_HEN[1]*qCos(EA[0])*qSin(EA[1]) + p_HEN[2]*qCos(EA[0])*qCos(EA[1]);
            //Receiver L1 L2 offset correction
            double Lamta1 = M_C/F1,Lamta2 = M_C/F2;
            double L1Offset = 0,L2Offset = 0;
            if( spp_pos[0] !=0 ) getRecvOffset(EA,tempSatlitData.SatType,L1Offset,L2Offset, tempSatlitData.wantObserType);
            tempSatlitData.L1Offset = L1Offset/Lamta1;
            tempSatlitData.L2Offset = L2Offset/Lamta2;
            //Satellite antenna phase center correction
            double SatL12Offset[2] = {0};
            if( spp_pos[0] !=0 )
                getSatlitOffset(epochTime.Year,epochTime.Month,epochTime.Day,
                    epochTime.Hours,epochTime.Minutes,epochTime.Seconds - tempSatlitData.C2/M_C,
                                         tempSatlitData.PRN,tempSatlitData.SatType,pXYZ,spp_pos, SatL12Offset, tempSatlitData.wantObserType);//pXYZ saves satellite coordinates
            tempSatlitData.SatL1Offset = SatL12Offset[0] / Lamta1;
            tempSatlitData.SatL2Offset = SatL12Offset[0] / Lamta2;
            //Calculate tide correction
            tempSatlitData.TideEffect = 0;
            //Calculate antenna phase winding
            double AntWindup = 0,preAntWindup = 0;
            //Find the previous epoch. Is there a satellite present? The deposit is stored in preAntWindup or preAntWindup=0.
            if( spp_pos[0] !=0 )
            {
                preAntWindup = getPreEpochWindUp(prevEpochSatlitData,tempSatlitData.PRN,tempSatlitData.SatType);//Get the previous epoch of WindUp
                AntWindup = getWindup(epochTime.Year,epochTime.Month,epochTime.Day,
                    epochTime.Hours,epochTime.Minutes,epochTime.Seconds - tempSatlitData.C2/M_C,
                    pXYZ,spp_pos,preAntWindup,m_ReadAntClass.m_sunpos);
            }
            tempSatlitData.AntWindup = AntWindup;
            //Computation to eliminate ionospheric pseudorange and carrier combinations (here absorbed receiver carrier deflection and WindUp)
            double alpha1 = (F1*F1)/(F1*F1 - F2*F2),alpha2 = (F2*F2)/(F1*F1 - F2*F2);
            tempSatlitData.LL3 = alpha1*(tempSatlitData.L1 + tempSatlitData.L1Offset - tempSatlitData.AntWindup)*Lamta1 - alpha2*(tempSatlitData.L2 + tempSatlitData.L2Offset - tempSatlitData.AntWindup)*Lamta2;//Eliminate ionospheric carrier LL3
            tempSatlitData.PP3 = alpha1*(tempSatlitData.C1 + Lamta1*tempSatlitData.L1Offset) - alpha2*(tempSatlitData.C2 + Lamta2 *tempSatlitData.L2Offset);//Eliminate ionospheric carrier PP3
            // save data to currEpoch
            currEpoch.append(tempSatlitData);
        }
        // judge satilite number large 4
        if(currEpoch.length() < 4)
        {
            memset(spp_pos, 0, 3*sizeof(double));// debug by xiaogongwei 2019.09.25
            epochSatlitData = currEpoch;// debug by xiaogongwei 2019.04.10
            return ;
        }
        // get equation
        MatrixXd mat_B, mat_P;
        VectorXd Vct_L, Xk;
        Vector3d XYZ_Pos;
        Obtaining_equation( currEpoch, spp_pos, mat_B, Vct_L, mat_P);// debug xiaogongwei 2018.11.16
        // slover by least square
        Xk = (mat_B.transpose()*mat_P*mat_B).inverse()*mat_B.transpose()*mat_P*Vct_L;
        XYZ_Pos[0] = tempPos3d[0] + Xk[0];
        XYZ_Pos[1] = tempPos3d[1] + Xk[1];
        XYZ_Pos[2] = tempPos3d[2] + Xk[2];
        diff_3d = XYZ_Pos - tempPos3d;
        tempPos3d = XYZ_Pos;// save slover pos
        // update spp_pos
        spp_pos[0] = XYZ_Pos[0]; spp_pos[1] = XYZ_Pos[1]; spp_pos[2] = XYZ_Pos[2];
        // debug by xiaogongwei 2018.11.17
        if(diff_3d.cwiseAbs().maxCoeff() < 1)
        {
            store_currEpoch = currEpoch;
            break;
        }
        if(diff_3d.cwiseAbs().maxCoeff() > 2e7 || !isnormal(diff_3d[0]) || iterj == max_iter - 1)
        {
            memset(spp_pos, 0, 3*sizeof(double));
            epochSatlitData = currEpoch;// debug by xiaogongwei 2019.09.25
            return ;
        }
    }// for(int iterj = 0; iterj < 20; iterj++)
// add Pesudorange smoothed by xiaogongwei 2018.11.20
    MatrixXd mat_B, mat_P;
    VectorXd Vct_L, Xk_smooth;
    //Monitor satellite quality and cycle slip
//    getGoodSatlite(prevEpochSatlitData,store_currEpoch, m_CutAngle);
    if(store_currEpoch.length() < m_minSatFlag)
    {
        memset(spp_pos, 0, 3*sizeof(double));// debug by xiaogongwei 2019.09.25
        epochSatlitData = store_currEpoch;// debug by xiaogongwei 2019.04.10
        return ;
    }

    if(m_isSmoothRange)// Whether to use phase smoothing pseudorange for SPP
    {
        m_QPseudoSmooth.SmoothPesudoRange(prevEpochSatlitData, store_currEpoch);
        Obtaining_equation( store_currEpoch, spp_pos, mat_B, Vct_L, mat_P, true);// debug xiaogongwei 2018.11.16
        // slover by least square
        Xk_smooth = (mat_B.transpose()*mat_P*mat_B).inverse()*mat_B.transpose()*mat_P*Vct_L;
        // update spp_pos
        spp_pos[0] += Xk_smooth[0]; spp_pos[1] += Xk_smooth[1]; spp_pos[2] += Xk_smooth[2];
        // use spp_pos update  mat_B  Vct_L
        Obtaining_equation( store_currEpoch, spp_pos, mat_B, Vct_L, mat_P, true);// debug xiaogongwei 2019.03.28
    }
    else
    {// Safe operation
        for(int i = 0; i < store_currEpoch.length(); i++)
        {
            store_currEpoch[i].PP3_Smooth = store_currEpoch[i].PP3;
            store_currEpoch[i].PP3_Smooth_NUM = 1;
            store_currEpoch[i].PP3_Smooth_Q = 1 / store_currEpoch[i].SatWight;
        }
        Obtaining_equation( store_currEpoch, spp_pos, mat_B, Vct_L, mat_P, false);
    }
    // Qulity control. add by xiaogongwei 2019.05.06
    VectorXd delate_flag;
//    int max_iter1 = 10;
    // Assuming that SPP has only one gross error, use if is not while, filtering uses while loop to eliminate Debug by xiaogongwei  2019.05.06
    while(m_qualityCtrl.VtPVCtrl_C(mat_B, Vct_L, mat_P, delate_flag, store_currEpoch.length()))
    {
        QVector<int> del_val;
        int sat_len = store_currEpoch.length();
        for(int i = sat_len - 1; i >= 0;i--)
        {
            if(0 != delate_flag[i])
                del_val.append(i);
        }
//        max_iter1--;
//        if(max_iter1 <= 0) break;
        // delete gross Errors
        int del_len = del_val.length();
        if(sat_len - del_len >= 5)
        {
            for(int i = 0; i < del_len;i++)
                store_currEpoch.remove(del_val[i]);
            sat_len = store_currEpoch.length();// update epochLenLB

            //update spp_pos
            Obtaining_equation( store_currEpoch, spp_pos, mat_B, Vct_L, mat_P, m_isSmoothRange);
            MatrixXd mat_Q = (mat_B.transpose()*mat_P*mat_B).inverse();
            VectorXd x_solver = mat_Q*mat_B.transpose()*mat_P*Vct_L;
            spp_pos[0] += x_solver[0]; spp_pos[1] += x_solver[1]; spp_pos[2] += x_solver[2];
        }
        else
        {
            memset(spp_pos, 0, 3*sizeof(double));// debug by xiaogongwei 2019.09.25
            break;
        }
    }
// change epochSatlitData !!!!!!
    epochSatlitData = store_currEpoch;
}

//Read O files, sp3 files, clk files, and various error calculations, Kalman filtering ......................
//isDisplayEveryEpoch represent is disply every epoch information?(ENU or XYZ)
void QPPPBackSmooth::Run(bool isDisplayEveryEpoch)
{
    // next is use to back smooth fillter
    if(!m_haveObsFile) return ;// if not have observation file.
    QTime myTime; // set timer
    myTime.start();// start timer

    // Use PPP forward filtter
    QPPPModel *myPPP = new QPPPModel(m_run_floder, mp_QTextEditforDisplay, m_Solver_Method, m_SatSystem,
                    m_TropDelay, m_CutAngle, m_isKinematic, m_Smooth_Str);
    myPPP->Run(isDisplayEveryEpoch);// false represent no disply every epoch information(ENU or XYZ)
    m_isRuned = myPPP->isRuned();
    if(!m_isRuned) return;
    // get forward fillter information
    int forWard_epoch_len = myPPP->m_writeFileClass.allSolverX.length();
    if(forWard_epoch_len == 0) return ;
    QVector< RecivePos > last_allReciverPos = myPPP->m_writeFileClass.allReciverPos;
    QVector< VectorXd > last_allSolverX = myPPP->m_writeFileClass.allSolverX;
    QVector< MatrixXd > last_allSolverQ = myPPP->m_writeFileClass.allSloverQ;
    VectorXd last_fillter_X;
    MatrixXd last_fillter_Q;
    for(int i = forWard_epoch_len - 1; i >=0; i--)
    {
        last_fillter_X = last_allSolverX.at(i);
        if(last_fillter_X[0] != 0)
        {
            last_fillter_X = last_allSolverX.at(i);
            last_fillter_Q = last_allSolverQ.at(i);
            break;
        }
    }
    QVector< QVector < SatlitData > > last_allPPPSatlitData = myPPP->m_writeFileClass.allPPPSatlitData;
    // delete myPPP pointer
    delete myPPP;
    //not use accelerate in search sp3 and clk
    m_ReadSP3Class.ACCELERATE = 0;
    m_ReadClkClass.ACCELERATE = 0;
    //Externally initialize fixed variables to speed up calculations
    double p_HEN[3] = {0};//Get the antenna high
    m_ReadOFileClass.getAntHEN(p_HEN);
    //Traversing data one by one epoch, reading O file data
    QString disPlayQTextEdit = "";// display for QTextEdit
    QVector < SatlitData > prevEpochSatlitData;//Store satellite data of an epoch, use cycle slip detection（Put it on top or read multReadOFile epochs, the life cycle will expire when reading）
    double spp_pos[3] = {last_fillter_X[0], last_fillter_X[1], last_fillter_X[2]};// store back smooth pos
    QVector< QVector < SatlitData > > multepochSatlitData;//Store multiple epochs
    multepochSatlitData = last_allPPPSatlitData;// get all epoch data
    int all_epoch_len = multepochSatlitData.length();
//Back smooth Multiple epoch cycles
        for (int epoch_num = all_epoch_len - 1; epoch_num >= 0;epoch_num--)
        {
            QVector< SatlitData > epochSatlitData;//Temporary storage of uncalculated data for each epoch satellite
            epochSatlitData = multepochSatlitData.at(epoch_num);
            if(epochSatlitData.length() == 0) continue;
            GPSPosTime epochTime = epochSatlitData.at(0).UTCTime;//Obtaining observation time（Each satellite in the epoch stores the observation time）
            //Set the epoch of the satellite
            for(int i = 0;i < epochSatlitData.length();i++)
                epochSatlitData[i].UTCTime.epochNum = epoch_num;

            if(epoch_num == 0)
            {
                int a = 0;
            }
            // use spp compute postion and smooth pesudorange
            double temp_spp_pos[3] = {last_allReciverPos.at(epoch_num).spp_pos[0],
                                      last_allReciverPos.at(epoch_num).spp_pos[1],
                                      last_allReciverPos.at(epoch_num).spp_pos[2]};
            //Monitor satellite quality and cycle slip
            getGoodSatlite(prevEpochSatlitData,epochSatlitData, m_CutAngle);

            // The number of skipping satellites is less than m_minSatFlag
            // update at 2018.10.17 for less m_minSatFlag satellites at the begain observtion
            if(epochSatlitData.length() < m_minSatFlag || spp_pos[0] == 0)
            {
//                prevEpochSatlitData.clear();
                disPlayQTextEdit = "GPST: " + QString::number(epochTime.Hours) + ":" + QString::number(epochTime.Minutes)
                        + ":" + QString::number(epochTime.Seconds) ;
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
                disPlayQTextEdit = "Satellite number: " + QString::number(epochSatlitData.length())
                                        + " jump satellites number is less than 5.";
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
                // translation to ENU
                VectorXd ENU_Vct;
                double spp_vct[3] = {0};
                ENU_Vct.resize(32+3*epochSatlitData.length());
                ENU_Vct.fill(0);
                saveResult2Class(ENU_Vct, spp_vct, epochTime, epochSatlitData, epoch_num);
                continue;
            }

            // Choose solve method Kalman or SRIF
            MatrixXd P;
            VectorXd X;//分别为dX,dY,dZ,dT(Zenith tropospheric residual),dVt(Receiver clock)，N1,N2...Nm(Ambiguity)[dx,dy,dz,dTrop,dClock,N1,N2,...Nn]
            double spp_vct[3] = {0};// save pos before fillter
            X.resize(5+epochSatlitData.length());
            X.setZero();
            // use prior information
            if(epoch_num == all_epoch_len - 1)
            {
                X = last_fillter_X;
                P = last_fillter_Q;
                X[0] = 0;X[1] = 0;X[2] = 0;
            }
            // store spp position
            spp_vct[0] = temp_spp_pos[0]; spp_vct[1] = temp_spp_pos[1]; spp_vct[2] = temp_spp_pos[2];
            if (!m_Solver_Method.compare("SRIF", Qt::CaseInsensitive))
                m_SRIFAlgorithm.SRIFforStatic(prevEpochSatlitData,epochSatlitData,spp_pos,X,P);
            else
                m_KalmanClass.KalmanforStatic(prevEpochSatlitData,epochSatlitData,spp_pos,X,P);
//Save the last epoch satellite data
            prevEpochSatlitData = epochSatlitData;
//Output calculation result(print result)
            // display every epoch results
            if(isDisplayEveryEpoch)
            {
                int Valid_SatNumber = epochSatlitData.length();
                // display epoch number
                disPlayQTextEdit = "Epoch Number: " + QString::number(epoch_num);
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
                //
                disPlayQTextEdit = "GPST: " + QString::number(epochTime.Hours) + ":" + QString::number(epochTime.Minutes)
                        + ":" + QString::number(epochTime.Seconds) + ENDLINE + "Satellite number: " + QString::number(epochSatlitData.length());
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
                // display ENU or XYZ
                disPlayQTextEdit = "Valid Satellite Number: " + QString::number(Valid_SatNumber) + ENDLINE
                        + "Estimated coordinates: [ " + QString::number(spp_pos[0], 'f', 4) + "," + QString::number(spp_pos[1], 'f', 4) + ","
                        + QString::number(spp_pos[2], 'f', 4) + " ]" + ENDLINE;
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
            }
//Save each epoch X data to prepare for writing a file
            // translation to ENU
            VectorXd ENU_Vct;
            ENU_Vct = X;// [dx,dy,dz,dTrop,dClock,N1,N2,...Nn]
            //ENU_Vct[0] = P(1,1); ENU_Vct[1] = P(2,2); ENU_Vct[2] = P(3,3);
            ENU_Vct[0] = spp_pos[0]; ENU_Vct[1] = spp_pos[1]; ENU_Vct[2] = spp_pos[2];
            saveResult2Class(ENU_Vct, spp_vct, epochTime, epochSatlitData, epoch_num, &P);
        }//End of multiple epochs    for (int n = 0; n < multepochSatlitData.length();n++)

        // clear multepochSatlitData
        multepochSatlitData.clear();
// time end
    float m_diffTime = myTime.elapsed() / 1000.0;
    if(isDisplayEveryEpoch)
    {
        disPlayQTextEdit = "The Elapse Time: " + QString::number(m_diffTime) + "s";
        autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
    }
//Write result to file
    writeResult2File();
    m_isRuned = true;// Determine whether the operation is complete.
}


// The edit box automatically scrolls, adding one row or more lines at a time.
void QPPPBackSmooth::autoScrollTextEdit(QTextEdit *textEdit,QString &add_text)
{
    if(textEdit == NULL) return ;
    int m_Display_Max_line = 99999;
    //Add line character and refresh edit box.
    QString insertText = add_text + ENDLINE;
    textEdit->insertPlainText(insertText);
    //Keep the editor in the last line of the cursor.
    QTextCursor cursor=textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    textEdit->setTextCursor(cursor);
    textEdit->repaint();
    QApplication::processEvents();
    //If you exceed a certain number of lines, empty it.
    if(textEdit->document()->lineCount() > m_Display_Max_line)
    {
        textEdit->clear();
    }
}

bool QPPPBackSmooth::connectHost()
{
    if(m_isConnect) return true;
    QString ftp_link = "cddis.gsfc.nasa.gov", user_name = "anonymous", user_password = "";//ftp information
    int Port = 21;
    ftpClient.FtpSetUserInfor(user_name, user_password);
    ftpClient.FtpSetHostPort(ftp_link, Port);
    m_isConnect = true;
    return true;
}
//download Get sp3, clk, erp files
// productType: "sp3", "clk"
// GPS_Week, GPS_Day: is GPS week and day.
// fileList: if fileList least 3. down new product and change fileList
QStringList QPPPBackSmooth::downProducts(QString store_floder_path, int GPS_Week, int GPS_day, QString productType)
{
    QString igs_short_path = "/pub/gps/products/", igs_productCompany = "igs";// set path of products
    QString gbm_short_path = "/pub/gps/products/mgex/", gbm_productCompany = "gbm";// set path of products
    QStringList fileList;
    fileList.clear();
    if(!m_isConnect) return fileList;
    //get fileList
    QStringList net_fileList, igs_net_fileList;
    net_fileList.clear();
    igs_net_fileList.clear();
    QString tempStr = "", fileName = "", disPlayQTextEdit = "";
    QStringList three_fileNames, igs_three_fileNames;

    for(int i = -1;i < 2; i++)
    {
        int temp_week = GPS_Week, temp_day = GPS_day + i;
        if(temp_day < 0)
        {
            temp_week--;
            temp_day = 0;
        }
        else if(temp_day > 6)
        {
            temp_week++;
            temp_day = 0;
        }
        // get gbm products
        tempStr.append(gbm_short_path);
        tempStr.append(QString::number(temp_week));
        tempStr.append("/");

        fileName.append(gbm_productCompany);
        fileName.append(QString::number(temp_week));
        fileName.append(QString::number(temp_day));

        if (productType.contains("sp3", Qt::CaseInsensitive))
            fileName.append(".sp3.Z");
        else if (productType.contains("clk", Qt::CaseInsensitive))
            fileName.append(".clk.Z");
        else
            return fileList;
        tempStr.append(fileName); // save filename and ftp path
        three_fileNames.append(fileName);
        net_fileList.append(tempStr);
        // clear temp data
        tempStr = ""; fileName = "";

        // get igs products
        tempStr.append(igs_short_path);
        tempStr.append(QString::number(temp_week));
        tempStr.append("/");

        fileName.append(igs_productCompany);
        fileName.append(QString::number(temp_week));
        fileName.append(QString::number(temp_day));

        if (productType.contains("sp3", Qt::CaseInsensitive))
            fileName.append(".sp3.Z");
        else if (productType.contains("clk", Qt::CaseInsensitive))
            fileName.append(".clk.Z");
        else
            return fileList;
        tempStr.append(fileName); // save filename and ftp path
        igs_three_fileNames.append(fileName);
        igs_net_fileList.append(tempStr);
        // clear temp data
        tempStr = ""; fileName = "";

    }
    // down three files
    bool is_down_gbm = false, is_down_igs = false;
    disPlayQTextEdit = "download start!";
    autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
    for(int i = 0; i < 3;i++)
    {
        QString gbm_temp_local_file = store_floder_path, igs_temp_local_file = store_floder_path;
        gbm_temp_local_file.append(three_fileNames.at(i));
        igs_temp_local_file.append(igs_three_fileNames.at(i));
        if(!ftpClient.FtpGet(net_fileList.at(i), gbm_temp_local_file))
        {
            disPlayQTextEdit = "download: " + net_fileList.at(i) + " bad!";
            autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
            if(!ftpClient.FtpGet(igs_net_fileList.at(i), igs_temp_local_file))
            {
                disPlayQTextEdit = "download: " + igs_net_fileList.at(i) + " bad!";
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
                is_down_igs = false;
            }
            else
            {
                disPlayQTextEdit = "download: " + igs_net_fileList.at(i) + " success!";
                autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
                is_down_igs = true;
            }
        }
        else
        {
            is_down_gbm = true;
            disPlayQTextEdit = "download: " + net_fileList.at(i) + " success!";
            autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
        }
        // save products file to fileList
        QString temp_local_file;
        if(is_down_gbm)
            temp_local_file = gbm_temp_local_file;
        else if(is_down_igs)
            temp_local_file = igs_temp_local_file;
        // If the download fail
        if(!is_down_gbm && !is_down_igs)
        {
            m_haveObsFile = false;
            disPlayQTextEdit = "download: gbm or igs products bad, Procedure will terminate!";
            autoScrollTextEdit(mp_QTextEditforDisplay, disPlayQTextEdit);// display for QTextEdit
        }
        int extend_name = temp_local_file.lastIndexOf(".");
        QString product_name = temp_local_file.mid(0, extend_name);
        fileList.append(product_name);
        // umcompress ".Z"
        QFileInfo fileinfo(temp_local_file);
        if(0 != fileinfo.size())
        {
            MyCompress compress;
            compress.UnCompress(temp_local_file, store_floder_path);
        }
    }
    return fileList;
}

//Destructor
QPPPBackSmooth::~QPPPBackSmooth()
{
}

//Setting up the file system SystemStr:"G"(Turn on the GPS system);"GR":(Turn on the GPS+GLONASS system);"GRCE"(Open all)et al
//GPS, GLONASS, BDS, and Galieo are used respectively: the letters G, R, C, E
bool QPPPBackSmooth::setSatlitSys(QString SystemStr)
{
	bool IsGood = QBaseObject::setSatlitSys(SystemStr);
	//Set to read O files
	m_ReadOFileClass.setSatlitSys(SystemStr);
	//Set to read Sp3
	m_ReadSP3Class.setSatlitSys(SystemStr);
	//Set to read Clk files
	m_ReadClkClass.setSatlitSys(SystemStr);
	//Setting up the receiver satellite antenna system
	m_ReadAntClass.setSatlitSys(SystemStr);
	//Set kalman filtering system
	m_KalmanClass.setSatlitSys(SystemStr);
    //Set the system of m_writeFileClass
    m_writeFileClass.setSatlitSys(SystemStr);
    //Set the system of m_SRIFAlgorithm
    m_SRIFAlgorithm.setSatlitSys(SystemStr);
	return IsGood;
}

//Obtain the coordinates of the epoch satellite from the SP3 data data
void QPPPBackSmooth::getSP3Pos(double GPST,int PRN,char SatType,double *p_XYZ,double *pdXYZ)
{
	m_ReadSP3Class.getPrcisePoint(PRN,SatType,GPST,p_XYZ,pdXYZ);
}

//Get clock error from CLK data
void QPPPBackSmooth::getCLKData(int PRN,char SatType,double GPST,double *pCLKT)
{
	m_ReadClkClass.getStaliteClk(PRN,SatType,GPST,pCLKT);
}

//Earth autobiography correction
double QPPPBackSmooth::getSagnac(double X,double Y,double Z,double *approxRecvXYZ)
{//Calculate the autobiographic correction of the earth
	double dltaP = M_We*((X - approxRecvXYZ[0])*Y - (Y - approxRecvXYZ[1])*X)/M_C;
	return -dltaP;//Returns the opposite number such that p = p' + dltaP; can be added directly
}

void QPPPBackSmooth::getWight(SatlitData &tempSatlitData)
{
    double E = tempSatlitData.EA[0]*MM_PI/180;
    double SatWight = qSin(E)*qSin(E) / M_Zgama_P_square;//Set the weight of the satellite
    switch (tempSatlitData.SatType) {
    case 'R': SatWight = 0.75*SatWight; break;
    case 'C': case 'E': SatWight = 0.3*SatWight; break;
    default:
        break;
    }
    //Five satellites with poor GEO orbit in front of Beidou
    if(tempSatlitData.SatType == 'C' && tempSatlitData.PRN <= 5)
        SatWight = 0.01*SatWight;
    tempSatlitData.SatWight = SatWight;//Set the weight of the satellite    debug by xiaogongwei 2019.04.24
}

//Computational relativistic effect
double QPPPBackSmooth::getRelativty(double *pSatXYZ,double *pRecXYZ,double *pSatdXYZ)
{
	/*double c = 299792458.0;
	double dltaP = -2*(pSatXYZ[0]*pSatdXYZ[0] + pSatdXYZ[1]*pdXYZ[1] + pSatXYZ[2]*pSatdXYZ[2]) / c;*/
	double b[3] = {0},a = 0,R = 0,Rs = 0,Rr = 0,v_light = 299792458.0,GM=3.9860047e14,dltaP = 0;
	b[0] = pRecXYZ[0] - pSatXYZ[0];
	b[1] = pRecXYZ[1] - pSatXYZ[1];
	b[2] = pRecXYZ[2] - pSatXYZ[2];
	a = pSatXYZ[0]*pSatdXYZ[0] + pSatXYZ[1]*pSatdXYZ[1] + pSatXYZ[2]*pSatdXYZ[2];
	R=qCmpGpsT.norm(b,3);
	Rs = qCmpGpsT.norm(pSatXYZ,3);
	Rr = qCmpGpsT.norm(pRecXYZ,3);
	dltaP=-2*a/M_C + (2*M_GM/qPow(M_C,2))*qLn((Rs+Rr+R)/(Rs+Rr-R));
	return dltaP;//m
}

//Calculate EA, E: satellite elevation angle, A: azimuth
void QPPPBackSmooth::getSatEA(double X,double Y,double Z,double *approxRecvXYZ,double *EA)
{//Calculate EA//appear BUG Since XYZ to BLH is calculated, L (earth longitude) is actually opposite when y < 0, x > 0.L = -atan(y/x) error should be L = -atan(-y/x)
	double pSAZ[3] = {0};
	qCmpGpsT.XYZ2SAE(X,Y,Z,pSAZ,approxRecvXYZ);//appear Bug
	EA[0] = (MM_PI/2 - pSAZ[2])*360/(2*MM_PI);
	EA[1] = pSAZ[1]*360/(2*MM_PI);
}


//Using the Sass model There are other models and projection functions that can be used, as well as the GPT2 model.
// ZHD_s: GNSS signal direction dry delay, ZHD: Station zenith direction dry delay
void QPPPBackSmooth::getTropDelay(double MJD,int TDay,double E,double *pBLH,double *mf, double *ZHD_s, double *ZPD)
{
    //double GPT2_Trop = m_ReadTropClass.getGPT2SasstaMDelay(MJD,TDay,E,pBLH,mf);//The GPT2 model only returns the dry delay estimate and the wet delay function.
    double tropDelayH = 0, tropDelayP = 0;
    if(m_TropDelay.mid(0,1).compare("U") == 0)
    {
        if(ZPD)     m_ReadTropClass.getUNB3mDelay(pBLH,TDay,E,mf, &tropDelayP);//Total delay of the UNB3M model
        if(ZHD_s)     tropDelayH = m_ReadTropClass.getUNB3mDelay(pBLH,TDay,E,mf);//UNB3M model only returns dry delay
    }
    else if(m_TropDelay.mid(0,1).compare("S") == 0)
    {
        if(ZPD)     m_ReadTropClass.getGPT2SasstaMDelay(MJD,TDay,E,pBLH,mf, &tropDelayP);//GPT2 model total delay
        if(ZHD_s)     tropDelayH = m_ReadTropClass.getGPT2SasstaMDelay(MJD,TDay,E,pBLH,mf);//GPT2 model only returns dry delay
    }
    else
    {
        if(ZPD)     m_ReadTropClass.getGPT2HopfieldDelay(MJD,TDay,E,pBLH,mf, &tropDelayP);//GPT2 model total delay
        if(ZHD_s)     tropDelayH = m_ReadTropClass.getGPT2HopfieldDelay(MJD,TDay,E,pBLH,mf);//GPT2 model only returns dry delay
    }

    // juge tropDely is nan or no Meaningless
    if(qAbs(tropDelayH) > 100  || qAbs(pBLH[2]) > 50000)
        tropDelayH = 1e-6;
    else if(!isnormal(tropDelayH))
        tropDelayH = 1e-6;
    if(qAbs(tropDelayP) > 100  || qAbs(pBLH[2]) > 50000)
        tropDelayP = 1e-6;
    else if(!isnormal(tropDelayP))
        tropDelayP = 1e-6;

    if(ZHD_s)  *ZHD_s = tropDelayH;
    if(ZPD)  *ZPD = tropDelayP;
    return ;
}


//Calculate the receivers L1 and L2 phase center correction PCO + PCV, L1Offset and L2Offset represent the distance correction of the line of sight direction
bool QPPPBackSmooth::getRecvOffset(double *EA,char SatType,double &L1Offset,double &L2Offset, QVector<QString> FrqFlag)
{
    if (m_ReadAntClass.getRecvL12(EA[0],EA[1],SatType,L1Offset,L2Offset, FrqFlag))
        return true;
    else
    {
        L1Offset = 0; L2Offset = 0;
        return false;
    }
}

//Calculate the satellite PCO+PCV correction, because the satellite G1 and G2 frequencies are the same, so the two bands change exactly the same; StaPos and RecPos, satellite and receiver WGS84 coordinates (unit m)
//L12Offset
bool QPPPBackSmooth::getSatlitOffset(int Year,int Month,int Day,int Hours,int Minutes,double Seconds,int PRN,char SatType,double *StaPos,double *RecPos,
                                  double *L12Offset, QVector<QString> FrqFlag)
{
    bool isGood = m_ReadAntClass.getSatOffSet(Year,Month,Day,Hours,Minutes,Seconds,PRN,SatType,StaPos,RecPos, L12Offset, FrqFlag);//pXYZ saves satellite coordinates
    if(isGood)
        return true;
    {
        L12Offset[0] = 0; L12Offset[1] = 0;
        return false;
    }

}

//Calculate the correction of the tide in the direction of the line of sight (unit m)
double QPPPBackSmooth::getTideEffect(int Year,int Month,int Day,int Hours,int Minutes,double Seconds,
                                double *pXYZ,double *EA,double *psunpos/* =NULL */, double *pmoonpos /* = NULL */,double gmst /* = 0 */,QString StationName /* = "" */)
{
	return m_TideEffectClass.getAllTideEffect(Year,Month,Day,Hours,Minutes,Seconds,pXYZ,EA,psunpos,pmoonpos,gmst,StationName);
}

//SatPos and RecPos represent the satellite and receiver WGS84 coordinates. Return to the weekly (unit week) range [-0.5 +0.5]
double QPPPBackSmooth::getWindup(int Year,int Month,int Day,int Hours,int Minutes,double Seconds,double *StaPos,double *RecPos,double &phw,double *psunpos)
{
	return m_WinUpClass.getWindUp(Year,Month,Day,Hours,Minutes,Seconds,StaPos,RecPos,phw,psunpos);
}


//Detect cycle hops: return pLP is a three-dimensional array, the first is the W-M combination (N2-N1 < 3.5) The second number ionospheric residual (<0.3) The third is (lamt2*N2-lamt1*N1 < 3.5)
bool QPPPBackSmooth::CycleSlip(const SatlitData &oneSatlitData,double *pLP)
{//
    if (oneSatlitData.L1*oneSatlitData.L2*oneSatlitData.C1*oneSatlitData.C2 == 0)//Determine whether it is dual-frequency data
        return false;
    double F1 = oneSatlitData.Frq[0],F2 = oneSatlitData.Frq[1];//Get the frequency of this satellite
    double Lamta1 = M_C/F1,Lamta2 = M_C/F2;
    //MW Combination(1)
//    double NL12 = ((F1-F2)/(F1+F2))*(oneSatlitData.C1/Lamta1 + oneSatlitData.C2/Lamta2) - (oneSatlitData.L1 - oneSatlitData.L2);
    //MW Combination(2)
    double lamdaMW = M_C/(F1 - F2);
    double NL12 = (oneSatlitData.L1 - oneSatlitData.L2) - (F1*oneSatlitData.C1 + F2*oneSatlitData.C2)/((F1+F2)*lamdaMW);
    double IocL = Lamta1*oneSatlitData.L1 - Lamta2*oneSatlitData.L2;//Ionospheric delayed residual
    double IocLP = IocL+(oneSatlitData.C1 - oneSatlitData.C2);
    pLP[0] = NL12;
    pLP[1] =IocL;
    pLP[2] = IocLP;
    return true;
}

//Save the phase entanglement of the previous epoch
double QPPPBackSmooth::getPreEpochWindUp(QVector< SatlitData > &prevEpochSatlitData,int PRN,char SatType)
{
	int preEopchLen = prevEpochSatlitData.length();
	if (0 == preEopchLen) return 0;

	for (int i = 0;i < preEopchLen;i++)
	{
		SatlitData oneSatalite = prevEpochSatlitData.at(i);
		if (PRN == oneSatalite.PRN&&oneSatalite.SatType == SatType)
			return oneSatalite.AntWindup;
	}
	return 0;
}


//Repair receiver clock jump
void QPPPBackSmooth::reciveClkRapaire(QVector< SatlitData > &prevEpochSatlitData,QVector< SatlitData > &epochSatlitData)
{
    int preEpochLen = prevEpochSatlitData.length();
    int epochLen = epochSatlitData.length();
    // clock jump repair Debug by xiaogongwei 2019.03.30
    double sum_S = 0.0, k1 = 0.01*M_C, M = 0.0, jump = 0.0;// for repair clock
    int clock_num = 0, check_sat_num = 0;// for repair clock
    for (int i = 0;i < epochLen;i++)
    {
        SatlitData epochData = epochSatlitData.at(i);
        for (int j = 0;j < preEpochLen;j++)
        {
            SatlitData preEpochData = prevEpochSatlitData.at(j);
            if (epochData.PRN == preEpochData.PRN&&epochData.SatType == preEpochData.SatType)
            {
                check_sat_num++;
                double Lamta1 = M_C / preEpochData.Frq[0], Lamta2 = M_C / preEpochData.Frq[1];
                double IocL1 = Lamta1*preEpochData.L1 - Lamta2*preEpochData.L2,
                        IocL2 = Lamta1*epochData.L1 - Lamta2*epochData.L2;//Ionospheric delayed residual
                if(qAbs(IocL2 - IocL1) < M_IR)
                {
                    double dP3 = epochData.PP3 - preEpochData.PP3, dL3 = epochData.LL3 - preEpochData.LL3;
                    double Si = (dP3 - dL3);// (m)
                    // judge clock jump type
                    if(qAbs(dP3) >= 0.001*M_C && m_clock_jump_type == 0)
                        m_clock_jump_type = 1;
                    if(qAbs(dL3) >= 0.001*M_C && m_clock_jump_type == 0)
                        m_clock_jump_type = 2;
                    if(qAbs(Si) >= 0.001*M_C)
                    {
                        sum_S += Si;
                        clock_num++;
                    }
                }
            }
        }

    }
    // repair clock
    double jump_pro = (double)clock_num / check_sat_num;
    if(jump_pro > 0.8)
    {
        M = 1e3*sum_S / (M_C*clock_num);// unit:ms
        if(qAbs(qRound(M) - M) <= 1e-5)
            jump = qRound(M);
        else
            jump = 0;
    }
    if(jump != 0)
    {
        for (int i = 0;i < epochLen;i++)
        {
            SatlitData tempSatlitData = epochSatlitData[i];
            double F1 = tempSatlitData.Frq[0],F2 = tempSatlitData.Frq[1];
            double Lamta1 = M_C/F1,Lamta2 = M_C/F2;
            double alpha1 = (F1*F1)/(F1*F1 - F2*F2),alpha2 = (F2*F2)/(F1*F1 - F2*F2);
            if(m_clock_jump_type == 1)
            {// clock jump type I
                tempSatlitData.L1 = tempSatlitData.L1 + 1e-3*jump*M_C / Lamta1;
                tempSatlitData.L2 = tempSatlitData.L2 + 1e-3*jump*M_C / Lamta2;
                tempSatlitData.LL3 = alpha1*(tempSatlitData.L1 + tempSatlitData.L1Offset + tempSatlitData.SatL1Offset - tempSatlitData.AntWindup)*Lamta1
                        - alpha2*(tempSatlitData.L2 + tempSatlitData.L2Offset + tempSatlitData.SatL2Offset - tempSatlitData.AntWindup)*Lamta2;//Eliminate ionospheric carrier LL3

            }
            else if(m_clock_jump_type == 2)
            {// clock jump type II
                tempSatlitData.C1 = tempSatlitData.C1 - 1e-3*jump*M_C / Lamta1;
                tempSatlitData.C2 = tempSatlitData.C2 - 1e-3*jump*M_C / Lamta2;
                tempSatlitData.PP3 = alpha1*(tempSatlitData.C1 + Lamta1*tempSatlitData.L1Offset + Lamta1*tempSatlitData.SatL1Offset)
                        - alpha2*(tempSatlitData.C2 + Lamta2 *tempSatlitData.L2Offset + Lamta2*tempSatlitData.SatL2Offset);//Eliminate ionospheric carrier PP3
            }
            epochSatlitData[i] = tempSatlitData;
        }
    }
}

//Screening satellites that do not have missing data and detect cycle slips, high quality (height angles, ranging codes, etc.)
void QPPPBackSmooth::getGoodSatlite(QVector< SatlitData > &prevEpochSatlitData,QVector< SatlitData > &epochSatlitData,double eleAngle)
{
    int preEpochLen = prevEpochSatlitData.length();
    int epochLen = epochSatlitData.length();

    reciveClkRapaire(prevEpochSatlitData, epochSatlitData);

    //Cycle slip detection
    QVector< int > CycleFlag;//Record the position of the weekly jump
    CycleFlag.resize(epochLen);
    for (int i = 0;i < epochLen;i++) CycleFlag[i] = 0;
    for (int i = 0;i < epochLen;i++)
    {
        SatlitData epochData = epochSatlitData.at(i);
        //Data is not 0
        if (!(epochData.L1&&epochData.L2&&epochData.C1&&epochData.C2)) // debug xiaogongwei 2018.11.16
            CycleFlag[i] = -1;
        //The corrections are not zero
        if (!(epochData.X&&epochData.Y&&epochData.Z&&epochData.StaClock)) // debug xiaogongwei 2018.11.16
            CycleFlag[i] = -1;
        //Quality control (height angle, pseudorange difference)
        if (epochData.EA[0] < eleAngle || qAbs(epochData.C1 - epochData.C2) > 50)
            CycleFlag[i] = -1;
        // signal intensity
        if(epochData.SigInten >0 && epochData.SigInten < 0)
            CycleFlag[i] = -1;
        //Cycle slip detection
        for (int j = 0;j < preEpochLen;j++)//(jump == 0) not happen clock jump  && (jump == 0)
        {
            SatlitData preEpochData = prevEpochSatlitData.at(j);
            if (epochData.PRN == preEpochData.PRN&&epochData.SatType == preEpochData.SatType)
            {//Need to judge the system
                double epochLP[3]={0},preEpochLP[3]={0},diffLP[3]={0};
                CycleSlip(epochData,epochLP);
                CycleSlip(preEpochData,preEpochLP);
                for (int n = 0;n < 3;n++)
                    diffLP[n] = qAbs(epochLP[n] - preEpochLP[n]);
                //Determine the weekly jump threshold based on experience.
                // diffLP[2] No longer use 2011.08.24,  diffLP[1] The ionospheric residual threshold is set to 0.1m
                if (diffLP[0] > 5 ||diffLP[1] > M_IR||diffLP[2] > 99999 || qAbs(epochData.AntWindup - preEpochData.AntWindup) > 0.3)
                {//Weekly jump
                    CycleFlag[i] = -1;//Save the weekly jump satellite logo
                }
                break;
            }
            else
            {
                continue;
            }
        }
    }
    //Remove low quality and weekly hop satellites
    QVector< SatlitData > tempEpochSatlitData;
    for (int i = 0;i < epochLen;i++)
    {
        if (CycleFlag.at(i) != -1)
        {
            tempEpochSatlitData.append(epochSatlitData.at(i));
        }
    }
    epochSatlitData = tempEpochSatlitData;

}

void QPPPBackSmooth::saveResult2Class(VectorXd X, double *spp_vct, GPSPosTime epochTime, QVector< SatlitData > epochResultSatlitData,
                                      int epochNum, MatrixXd *P)
{
    //Store coordinate data
    RecivePos epochRecivePos;
    epochRecivePos.Year = epochTime.Year;epochRecivePos.Month = epochTime.Month;
    epochRecivePos.Day = epochTime.Day;epochRecivePos.Hours = epochTime.Hours;
    epochRecivePos.Minutes = epochTime.Minutes;epochRecivePos.Seconds = epochTime.Seconds;

    epochRecivePos.totolEpochStalitNum = epochResultSatlitData.length();
    epochRecivePos.dX = X[0];
    epochRecivePos.dY = X[1];
    epochRecivePos.dZ = X[2];
    epochRecivePos.spp_pos[0] = spp_vct[0];
    epochRecivePos.spp_pos[1] = spp_vct[1];
    epochRecivePos.spp_pos[2] = spp_vct[2];
    m_writeFileClass.allReciverPos.prepend(epochRecivePos);
    //Save wet delay and receiver clock error
    double epoch_ZHD = 0.0;
    int const_num = 4 + m_sys_num;
    if(epochResultSatlitData.length() >= m_minSatFlag) epoch_ZHD = epochResultSatlitData.at(0).UTCTime.TropZHD;
    ClockData epochRecClock;
    epochRecClock.UTCTime.epochNum = epochNum;
    epochRecClock.UTCTime.Year = epochRecivePos.Year;epochRecClock.UTCTime.Month = epochRecivePos.Month;epochRecClock.UTCTime.Day = epochRecivePos.Day;
    epochRecClock.UTCTime.Hours = epochRecivePos.Hours;epochRecClock.UTCTime.Minutes = epochRecivePos.Minutes;epochRecClock.UTCTime.Seconds = epochRecivePos.Seconds;
    epochRecClock.ZTD_W = X(3) + epoch_ZHD;//Storage zenith wet delay + zenith dry delay
    // save clock
    memset(epochRecClock.clockData, 0, 6*sizeof(double));
    //Store the receiver skew of the first system, and the relative offset of other systems. GCRE
    for(int i = 0;i < m_sys_str.length();i++)
    {
        switch (m_sys_str.at(i).toLatin1()) {
        case 'G':
            epochRecClock.clockData[0] = X(4+i);
            break;
        case 'C':
            epochRecClock.clockData[1] = X(4+i);
            break;
        case 'R':
            epochRecClock.clockData[2] = X(4+i);
            break;
        case 'E':
            epochRecClock.clockData[3] = X(4+i);
            break;
        default:
            break;
        }
    }
    m_writeFileClass.allClock.prepend(epochRecClock);
    //Save satellite ambiguity
    Ambiguity oneSatAmb;
    for (int i = 0;i < epochResultSatlitData.length();i++)
    {
        SatlitData oneSat = epochResultSatlitData.at(i);
        oneSatAmb.PRN = oneSat.PRN;
        oneSatAmb.SatType = oneSat.SatType;
        oneSatAmb.UTCTime = epochRecClock.UTCTime;
        oneSatAmb.isIntAmb = false;
        oneSatAmb.Amb = X(i+const_num);
        oneSatAmb.UTCTime.epochNum = epochNum;
        m_writeFileClass.allAmbiguity.prepend(oneSatAmb);
    }
    // save used satlite Information
    m_writeFileClass.allPPPSatlitData.prepend(epochResultSatlitData);
    // save solver X
    m_writeFileClass.allSolverX.prepend(X);
    // save P matrix
    if(P)
        m_writeFileClass.allSloverQ.prepend(*P);
//    else
//        m_writeFileClass.allSloverQ.prepend(MatrixXd::Identity(10,10));
}


void QPPPBackSmooth::writeResult2File()
{
    QString product_path = m_run_floder, ambiguit_floder;
    QString floder_name = "Products_BackSmooth_" + m_Solver_Method + "_Static_" + m_sys_str + PATHSEG;
    if(m_isKinematic)
        floder_name = "Products_BackSmooth_" + m_Solver_Method + "_Kinematic_" + m_sys_str + PATHSEG;
    product_path.append(floder_name);
    // save images path
    m_save_images_path = product_path;
    ambiguit_floder = product_path + QString("Ambiguity") + PATHSEG;
    m_writeFileClass.WriteEpochPRN(product_path, "Epoch_PRN.txt");
    m_writeFileClass.writeRecivePos2Txt(product_path, "position.txt");
    m_writeFileClass.writePPP2Txt(product_path, "Satellite_info.ppp");
    m_writeFileClass.writeClockZTDW2Txt(product_path, "ZTDW_Clock.txt");
    m_writeFileClass.writeAmbiguity2Txt(ambiguit_floder);//The path is .//Ambiguity//
    m_writeFileClass.writeRecivePosKML(product_path, "position.kml");// gernerate KML
}

// Get operation results( clear QWrite2File::allPPPSatlitData Because the amount of data is too large.)
void QPPPBackSmooth::getRunResult(PlotGUIData &plotData)
{
    int dataLen = m_writeFileClass.allReciverPos.length();
    if(dataLen == 0 || !m_isRuned) return ;
    if(!m_save_images_path.isEmpty())  plotData.save_file_path = m_save_images_path;

    for(int i = 0;i < dataLen;i++)
    {
        plotData.X.append(m_writeFileClass.allReciverPos.at(i).dX);
        plotData.Y.append(m_writeFileClass.allReciverPos.at(i).dY);
        plotData.Z.append(m_writeFileClass.allReciverPos.at(i).dZ);
        plotData.spp_X.append(m_writeFileClass.allReciverPos.at(i).spp_pos[0]);
        plotData.spp_Y.append(m_writeFileClass.allReciverPos.at(i).spp_pos[1]);
        plotData.spp_Z.append(m_writeFileClass.allReciverPos.at(i).spp_pos[2]);
        plotData.clockData.append(m_writeFileClass.allClock.at(i).clockData[0]);// GPS clock
        plotData.clockData_bias_1.append(m_writeFileClass.allClock.at(i).clockData[1]);//clock bias maybe is zero
        plotData.clockData_bias_2.append(m_writeFileClass.allClock.at(i).clockData[2]);//clock bias maybe is zero
        plotData.clockData_bias_3.append(m_writeFileClass.allClock.at(i).clockData[3]);//clock bias maybe is zero
        plotData.ZTD_W.append(m_writeFileClass.allClock.at(i).ZTD_W);
    }
}

