#pragma once
#pragma once
#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#include <QtCore/QCoreApplication>
#include <QDebug>
#include <QList>
#include <QDir>
#include <QFileInfo>
#include <gdal_priv.h>
#include <iostream>
using namespace std;
void nc2GeoTIFF(const char *fileName, const char *outFileName, const char *fieldName, double X0 = -1.0, double Y0 = -1.0);
double* CalCoor(double truelat1, double truelat2, double cenLat, double cenLon, double xLat, double yLon, double nx, double ny, double dx, double dy);
void readInputPars(int argc, char ** argv);
QList<QString> getFileNames(QString dirName);
int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	qDebug() << "Start...\n";
	readInputPars(argc, argv);
	qDebug() << "successful£¡\n";
	return a.exec();
}
QList<QString> getFileNames(QString dirName)
{
	QDir dirFile(dirName);
	QList<QFileInfo> filesInfo(dirFile.entryInfoList());
	QList<QString> filePaths;
	for (auto it = filesInfo.begin();it != filesInfo.end();it++)
	{
		if (it->fileName()=="." || it->fileName() == ".." || !it->suffix().isEmpty())
			continue;
		filePaths.push_back(it->fileName());
	}
	return filePaths;
}
void readInputPars(int argc, char ** argv)
{
	if (argc==1)
	{
		qDebug() << "Incorrect parameter input\n";
		return;
	}
	QMap<QString, QString> pars;
	for (int i=1;i<argc;i++)
	{
		if (QString(argv[i]) == "-i")
		{
			pars.insert("-i", argv[i + 1]);
			continue;
		}
		if (QString(argv[i]) == "-o")
		{
			pars.insert("-o", argv[i + 1]);
			continue;
		}
		if (QString(argv[i]) == "-f")
		{
			pars.insert("-f", argv[i + 1]);
			continue;
		}
		if (QString(argv[i]) == "-x")
		{
			pars.insert("-x", argv[i + 1]);
			continue;
		}
		if (QString(argv[i]) == "-y")
		{
			pars.insert("-y", argv[i + 1]);
			continue;
		}
	}
	if (!pars.contains("-i") || !pars.contains("-o") || !pars.contains("-f"))
	{
		qDebug() << "Incorrect parameter input\n";
		return;
	}
	QList<QString> fileNames = getFileNames(pars["-i"]);
	if (fileNames.isEmpty())
	{
		qDebug() << "The input directory is empty\n";
		return;
	}
	
	QByteArray fB = pars["-f"].toLatin1();
	const char *fieldName = fB.data();

	if (pars.contains("-x") && pars.contains("-y"))
	{
		double X0 = pars["-x"].toDouble();
		double Y0 = pars["-y"].toDouble();
		
		for (int i =0;i<fileNames.size();i++)
		{
			QByteArray inB = (pars["-i"]+"/"+fileNames[i]).toLatin1();
			const char *fileName = inB.data();

			QByteArray ouB = (pars["-o"] + "/" + fileNames[i]).toLatin1();
			const char *outFileName = ouB.data();
			nc2GeoTIFF(fileName, outFileName, fieldName, X0, Y0);
		}
		
	}
	else
	{
		for (int i = 0; i < fileNames.size(); i++)
		{
			QByteArray inB = (pars["-i"] + "/" + fileNames[i]).toLatin1();
			const char *fileName = inB.data();

			QByteArray ouB = (pars["-o"] + "/" + fileNames[i]+".tif").toLatin1();
			const char *outFileName = ouB.data();
			nc2GeoTIFF(fileName, outFileName, fieldName);
			cout << fileName<<endl;
			cout << outFileName << endl;
		}
	}
}

void nc2GeoTIFF(const char *fileName, const char *outFileName, const char *fieldName, double X0, double Y0)
{
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	
	GDALDataset *opdatasets = static_cast<GDALDataset *>(GDALOpen(fileName, GA_ReadOnly));
	
	if (opdatasets == NULL)
	{
		std::cout << "NC file can't open\n";
		return;
	}
	
	map<string, string> mapProperty;
	char** pSubdatase1t = GDALGetMetadata((GDALDatasetH)opdatasets, "");
	int iCoun = CSLCount(pSubdatase1t);
	for (int i = 0; i<iCoun; ++i)
	{
		//cout << pSubdatase1t[i] << endl;
		string tmpKey = string(pSubdatase1t[i]);
		tmpKey = tmpKey.substr(tmpKey.find_first_of("#") + 1, tmpKey.find_first_of("=") - (tmpKey.find_first_of("#") + 1));
		string tmpValue = string(pSubdatase1t[i]);
		tmpValue = tmpValue.substr(tmpValue.find_first_of("=") + 1);

		mapProperty.insert(map<string, string>::value_type(tmpKey, tmpValue.c_str()));
		//cout << tmpKey << "\t" << mapProperty.find(tmpKey)->second << endl;

	}


	vector<string> subDataPaths;
	vector<string> subDataDesc;
	vector<string> subDataNames;
	char** pSubdataset = GDALGetMetadata((GDALDatasetH)opdatasets, "SUBDATASETS");
	int iCount = CSLCount(pSubdataset);

	if (iCount <= 0)
	{
		std::cout << "The file has no subdatasets" << endl;
		GDALClose((GDALDatasetH)opdatasets);
	}
	for (int i = 0; i < iCount; i += 2)
	{
		string tmpstr = string(pSubdataset[i]);
		tmpstr = tmpstr.substr(tmpstr.find_first_of("=") + 1);

		string tmpdesc = string(pSubdataset[i + 1]);
		tmpdesc = tmpdesc.substr(tmpdesc.find_first_of("=") + 1);

		string tmpname = tmpstr.substr(tmpstr.find_last_of(":") + 1);

		subDataPaths.push_back(tmpstr);
		subDataDesc.push_back(tmpdesc);
		subDataNames.push_back(tmpname);

	}


	int varNumber = -1;
	for (int i = 0; i<subDataNames.size(); ++i)
	{
		if (subDataNames[i] == fieldName)
		{
			varNumber = i;
			break;
		}
	}
	//cout << "var is " << varNumber << endl;
	if (varNumber == -1)
	{
		std::cout << "The field does not exist! \n";
		GDALClose(GDALDatasetH(opdatasets));
		return;
	}
	std::cout << "The selected fields are: " << subDataPaths[varNumber].c_str() << endl;


	const char *ncName = subDataPaths[varNumber].c_str();

	GDALDataset *ncFiledataset = (GDALDataset *)GDALOpen(ncName, GA_ReadOnly);
	if (ncFiledataset == NULL)
	{
		std::cout << "Can't open field data\n";
		GDALClose(GDALDatasetH(opdatasets));
		return;
	}

	GDALRasterBand *myBnad = ncFiledataset->GetRasterBand(1);
	int XSize = myBnad->GetXSize();
	int YSize = myBnad->GetYSize();
	GDALDataType dataType = myBnad->GetRasterDataType();
	string clon = "STAND_LON";
	string clat = "MOAD_CEN_LAT";
	string trueLAT1 = "TRUELAT1";
	string trueLAT2 = "TRUELAT2";
	string resXstr = "DX";
	string resYstr = "DY";
	string C_LAT = "CEN_LAT";
	string C_LON = "CEN_LON";
	string nLon = "SOUTH-NORTH_PATCH_END_UNSTAG";
	string nLat = "WEST-EAST_PATCH_END_UNSTAG";

	double cen_lon = atof(mapProperty.find(clon)->second.c_str());
	double cen_lat = atof(mapProperty.find(clat)->second.c_str());
	double truelat1 = atof(mapProperty.find(trueLAT1)->second.c_str());
	double truelat2 = atof(mapProperty.find(trueLAT2)->second.c_str());
	double resX = atof(mapProperty.find(resXstr)->second.c_str());
	double resY = atof(mapProperty.find(resYstr)->second.c_str());
	double c_lat = atof(mapProperty.find(C_LAT)->second.c_str());
	double c_lon = atof(mapProperty.find(C_LON)->second.c_str());
	double nx = atof(mapProperty.find(nLat)->second.c_str());
	double ny = atof(mapProperty.find(nLon)->second.c_str());

	double *coorXY0;
	if (X0==-1.0 || Y0 ==-1.0)
	{
		coorXY0 = CalCoor(truelat1, truelat2, cen_lat, cen_lon, c_lat, c_lon, nx, ny, resX, resY);
		if (coorXY0 == NULL)
		{
			qDebug() << "Coordinate conversion failure£¡\n";
			GDALClose(GDALDatasetH(opdatasets));
			GDALClose(GDALDatasetH(ncFiledataset));
			return;
		}
		X0 = coorXY0[0];
		Y0 = coorXY0[1];

	}
;
	OGRSpatialReference oSRS;
	oSRS.importFromProj4("+proj=longlat +a=6370000 +b=6370000 +no_defs");
	oSRS.SetLCC(truelat2, truelat1, cen_lat, cen_lon, 0, 0);
	//oSRS.SetWellKnownGeogCS("WGS84");
	double adfTransform[6] = { Y0, resX, 0, X0, 0, resY };


	// Creating an output file
	GDALDriver *myDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
	GDALDataset *outFile = myDriver->Create(outFileName, XSize, YSize, 1, dataType, NULL);

	outFile->SetGeoTransform(adfTransform);

	char *pszWKT = NULL;
	oSRS.exportToWkt(&pszWKT);
	outFile->SetProjection(pszWKT);

	float *inBuffer = new float[XSize*YSize];
	myBnad->RasterIO(GF_Read, 0, 0, XSize, YSize, inBuffer, XSize, YSize, dataType, 0, 0);
	//qDebug() << ((float *)inBuffer)[0] << endl;

	float *outBuffer = new float[XSize*YSize];
	int n = 0;
	for (int r = YSize - 1; r >= 0; r--)
	{
		for (int c = 0; c<XSize; c++)
		{
			outBuffer[n] = inBuffer[r*XSize + c];
			n++;
		}
	}
	outFile->RasterIO(GF_Write, 0, 0, XSize, YSize, outBuffer, XSize, YSize, dataType, 1, { 0 }, 0, 0, 0);


	delete inBuffer;
	delete outBuffer;
	GDALClose(GDALDatasetH(opdatasets));
	GDALClose(GDALDatasetH(outFile));
	GDALClose(GDALDatasetH(ncFiledataset));
}

double* CalCoor(double truelat1, double truelat2, double cenLat, double cenLon, double xLat, double yLon, double nx, double ny, double dx, double dy)
{
	OGRSpatialReference oSRS, *polatlon;
	OGRCoordinateTransformation *poTransform;
	oSRS.importFromProj4("+proj=longlat +a=6370000 +b=6370000 +no_defs");
	std::cout << truelat1 << "  " << truelat2 << "  " << cenLat << "  " << cenLon << endl;
	oSRS.SetLCC(truelat1, truelat2, cenLat, cenLon, 0, 0);
	oSRS.SetWellKnownGeogCS("WGS84");
	polatlon = oSRS.CloneGeogCS();
	poTransform = OGRCreateCoordinateTransformation(polatlon, &oSRS);
	if (poTransform == NULL)
	{
		std::cout << "Coordinate conversion failure£¡\n";
		return NULL;
	}
	//double x, y;
	/*y = 116.44467;
	x = 40.251732;*/
	std::cout << xLat;
	std::cout << yLon;
	poTransform->Transform(1, &xLat, &yLon);
	std::cout << xLat;
	std::cout << yLon;
	//cout << xLat << "   " << yLon << endl;
	double x0 = -(nx - 1) / 2.0 * dx + yLon;
	double y0 = -(ny - 1) / 2.0*dy + xLat;

	double *coorsXY = new double[2];
	coorsXY[0] = x0;
	coorsXY[1] = y0;

	return coorsXY;
}