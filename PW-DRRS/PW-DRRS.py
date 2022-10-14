#coding=utf-8
import numpy as np
import glob
import gdal
import math
from sklearn import ensemble
import joblib
import os
from natsort import ns, natsorted
from sklearn.metrics import mean_absolute_error
from sklearn.metrics import mean_squared_error
import scipy.stats
from sklearn.metrics import r2_score
from sklearn import ensemble

def isInList(ls1,ls2):
    for num in ls1:
        if num in ls2:
            return True
    return False

def regression_method(model, XDirs,YDir,outDir,logF,mlDir,cellD):
   
    with open(logF, 'w') as f:
        f.write("")
    f.close()

    dirNames = os.listdir(XDirs)
    print(dirNames)
    XfileNamesLS=[]
    fileCount = 0
    for d in dirNames:
        files = natsorted(glob.glob(XDirs+"/"+d+"/*.tif"),alg=ns.PATH)
        if fileCount == 0:
            fileCount = len(files)
        elif len(files) != fileCount:
            print(XDirs+"/"+d+"：The number of files in the path is incorrect!")
            return
        XfileNamesLS.append(files)

    YfileNamesLS = natsorted(glob.glob(YDir+"/*.tif"),alg=ns.PATH)

    for num in range(fileCount):
        YfileName = YfileNamesLS[num]
        print("Y:\n"+YfileName)
        datasetY = gdal.Open(YfileName)
        if datasetY == None:
            print("{}open failed!".format(datasetY))
            return
        myBandY = datasetY.GetRasterBand(1)
        nXSizeY = datasetY.RasterXSize
        nYSizeY = datasetY.RasterYSize
        noDataY = myBandY.GetNoDataValue()
        projInfo = datasetY.GetProjection()
        geoTransform = datasetY.GetGeoTransform()
        dataY = myBandY.ReadAsArray(0, 0, nXSizeY, nYSizeY).astype(np.float)
        LSY = dataY.flatten()


        XYLS = []
        nodataLS = []
        print("X:" )
        for d in range(len(dirNames)):
            XfileName = XfileNamesLS[d][num]
            print(XfileName)
            datasetX = gdal.Open(XfileName)
            if datasetX == None:
                print("{}open failed".format(datasetX))
                del datasetY
                return
            myBandX = datasetX.GetRasterBand(1)
            nXSizeX = datasetX.RasterXSize
            nYSizeX = datasetX.RasterYSize
            if (nXSizeX != nXSizeY) or (nYSizeX != nYSizeY):
                print("The dimensions of the X and Y images are different!")
                del datasetX
                del datasetY
                return
            nodataLS.append(myBandX.GetNoDataValue())
            dataX = myBandX.ReadAsArray(0, 0, nXSizeX, nYSizeX).astype(np.float)
            LSX = dataX.flatten()
            XYLS.append(LSX)
            del datasetX
        del datasetY


        onlyNodataXLS = np.asarray(nodataLS).copy()
        onlyXLS = np.asarray(XYLS).copy()
        nodataLS.append(noDataY)
        XYLS.append(LSY)
        XYLSNP = np.asarray(XYLS)
        LSshape = XYLSNP.shape
        mat = []
        onlyXmat = []
        valueAndNodataLS =[] 
        for c in range(LSshape[1]):
       
            vLS = XYLSNP[:,c]
            if isInList(nodataLS,vLS) ==False:
                mat.append(vLS)
           
            if isInList(onlyNodataXLS,vLS[:-1]) ==False:
                valueAndNodataLS.append(1)
                onlyXmat.append(vLS[:-1])
            else:
                valueAndNodataLS.append(0)

        
        np.random.shuffle(mat)
        matNP = np.asarray(mat)

        nn = len(matNP)
        trainCount = int(nn * 0.7)+1
        if trainCount < 120:
            with open(logF, 'a') as f:
                f.write("Unable to training\n")
            f.close()
            continue
        print("The number of pixels:"+str(trainCount))
        trainMat = matNP[:trainCount]
        testMat = matNP[trainCount:]
        x_train = trainMat[:, :-1]
        y_train = trainMat[:, -1]
        x_test = testMat[:, :-1]
        y_test = testMat[:, -1]
        
        model.fit(x_train, y_train)
        outModePath = mlDir+"/"+str(YfileNamesLS[num][-23:-4])+"lr.model"
        joblib.dump(model, outModePath)
        result = model.predict(x_test)
  
        cellF = cellD+"/"+str(YfileNamesLS[num][-23:-4])+".txt"
        with open(cellF, 'w') as f:
                f.write("y_test\tpre_test\n")
                for i in range(len(result)):
                    f.write(str(y_test[i])+"\t"+str(result[i])+"\n")
                f.close()
      
        mae = mean_absolute_error(y_test, result)
        mse = mean_squared_error(y_test, result)
        r2 = r2_score(y_test, result)
        cor = scipy.stats.pearsonr(y_test,result)[0]
        print("MAE：{}".format(mae))
        print("RMSE：{}".format(math.sqrt(mse)))
        print("R2：{}".format(r2))
        print("Cor：{}".format(cor))
        with open(logF, 'a') as f:
            f.write(YfileNamesLS[num]+" MAE/RMSE/R2/Cor:"+str(mae)+","+str(mse)+","+str(r2)+","+str(cor)+"\n")
        f.close()
      
        predXMat = np.asarray(onlyXmat)
        preRes = model.predict(predXMat)

        if np.asarray(valueAndNodataLS).sum() != preRes.shape[0]:
            print("The number of fitted pixels does not correspond!")
            return
        t = -1
        for r in range(nYSizeY):
            for c in range(nXSizeY):
                index = r*nXSizeY + c
                if valueAndNodataLS[index] == 1:
                    t = t+1
                    dataY[r][c] = preRes[t]
                else:
                    dataY[r][c] = noDataY
        
        outFileName = outDir+"/"+(YfileNamesLS[num])[-33:]
        print(outFileName)
        myDriver = gdal.GetDriverByName("GTiff")
        if myDriver == None:
            print("Driver loading failed!")
            return
        outFile = myDriver.Create(outFileName, nXSizeY, nYSizeY, 1, gdal.GDT_Float32)
        if outFile == None:
            print("Failed to create the output file!")
            return
        outFile.GetRasterBand(1).WriteArray(dataY)
        outFile.SetGeoTransform(geoTransform)
        outFile.SetProjection(projInfo)
        outFile.GetRasterBand(1).SetNoDataValue(noDataY)
        outFile.FlushCache()
        outFile = None
        print('-' * 40)

    return


if __name__ =='__main__':
    modelName = ensemble.RandomForestRegressor(n_estimators=20,)  
    inXDirs = r"F:\PW-DRRS\X"
    inYDir = r"F:\PW-DRRS\Y"
    outResDir = r"F:\PW-DRRS\result"
    modelDir = r"F:\PW-DRRS\model"
    errorFile = r"F:\PW-DRRS\log.txt"
    cellValueDir = r"F:\PW-DRRS\cellValue"
    regression_method(modelName, inXDirs, inYDir, outResDir,errorFile,modelDir,cellValueDir)











