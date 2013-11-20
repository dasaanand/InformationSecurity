#include <OpenCV/OpenCV.h>
#include <OpenCV/cv.h>
#include <OpenCv/highgui.h>
#include <OpenCV/cv.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <omp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <Parameters.h>

#define getmin(a,b,c) a<b?a<c?0:2:b<c?1:2

using namespace std;

IplImage *coverImg, *orgImg, *embImg;
int *key, noOfBits, noOfBitsEmbedded = 0, noOfPixels=0, header;
bool *secretBits;
string carrierFileName, secretMsgFileName, keyFileName, embeddedFileName;
char * secretMsg;

struct passwd *pw = getpwuid(getuid());
string homedir = pw->pw_dir;
clock_t start, stop;


int bin2dec(int a[])
{
	int decValue = 0, n;
	for (n=0; n<8; n++) {
		decValue += a[n] * pow(2, n);
	}
	return decValue;
}

void dec2bin(int n)
{
	if (n <= 1) {
		secretBits[noOfBitsEmbedded++] = n;
		return;
	}
	secretBits[noOfBitsEmbedded++] = n%2;
	dec2bin(n/2);
	return;
}


char* string2constChar(string str)
{
	char *writable = new char[str.size() + 1];
	copy(str.begin(), str.end(), writable);
	writable[str.size()] = '\0';
	return writable;
}


void writeKey()
{
	int n = ceilf((float)noOfBits/noOfLSB) + 1, i;
	ofstream keyOF;
	keyOF.open(string2constChar(keyFileName));
	
	for (i=0; i<n; i++) {
		keyOF << key[i] << "\t";
	}
	keyOF.close();
}


void writeEmbeddedImage(IplImage* img)
{
	if(!cvSaveImage(string2constChar(embeddedFileName), img))
	{
		std::cout << "Cant save the file";
	}	
}

void setSecretMsg()
{
	string file = homedir + "/IS/MessageIn.txt";
	ifstream IF;
	int length;
	
	IF.open(string2constChar(file));      // open input file	
	IF.seekg(0, std::ios::end);    // go to the end
	length = IF.tellg();           // report location (this is the lenght)
	IF.seekg(0, std::ios::beg);    // go back to the beginning
	secretMsg = new char[length];    // allocate memory for a buffer of appropriate dimension
	IF.read(secretMsg, length);       // read the whole file into the buffer
	IF.close(); 
	
}

void getSecretMsg(char ch[])
{
	string file = homedir + "/IS/Out.txt";
	int i;
	ofstream OF;
	OF.open(string2constChar(file));
	
	while (ch[i] != '\0') {
		OF << ch[i++];		
	}
	OF.close();
}

void GenerateFiles()
{
	int j,k;
	string path = homedir + "/IS/SLSBTool";
	char * dir = string2constChar(path);
	mkdir(dir, 0777);
	
	
	embeddedFileName = path + "/";
	j=carrierFileName.find_last_of("/\\");
	j++;
	
	k=carrierFileName.find_last_of(".");
	
	embeddedFileName += carrierFileName.substr(j, k-j);
	keyFileName = embeddedFileName + "_KEY.txt";
	embeddedFileName += "_EMBED.";
	embeddedFileName += carrierFileName.substr(k+1, carrierFileName.length()-k);
}


void GenerateKey()
{
	int i, j, temp;
	key = (int *)malloc(noOfPixels*sizeof(int));	
	for (i=0; i<noOfPixels; i++) {
		key[i] = i;
	}
	
	omp_set_num_threads(200);
#pragma omp parallel for schedule(dynamic)
	for (i=0; i<noOfPixels; i++) {
		j = rand()%(i+1);
		temp = key[i];
		key[i] = key[j];
		key[j] = temp;
	}
}



IplImage* CarrierEngine ( IplImage* img )
{
	int width = img->width, height = img->height, step = img->widthStep;
	nChannels = img->nChannels;
	noOfPixels = width * height;
	int i = 0, j, k, l, m, n, o, colBin[8], color[3], diff[3];
	bool flag = 0;
	
	setSecretMsg();
	noOfBits = 8 * strlen(secretMsg);
	noOfBitsEmbedded = 0;
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	noOfBitsEmbedded = 0;
	for (i=0; i<strlen(secretMsg); i++) {
		dec2bin((int)secretMsg[i]);
		noOfBitsEmbedded = (i+1)*8;
	}
	
	noOfBitsEmbedded=0;
	noOfPixels = width*height;
	GenerateKey();
	GenerateFiles();
	
	uchar *data = ( uchar* )img->imageData;

	i = 0;
	while (noOfBitsEmbedded < noOfBits) {
		//extract data from key[i]th pixel
		j = (key[i]/step)*step;
		k = (key[i]%step)*nChannels;

		
		//embedd bits	
		for (m=0; m<nChannels; m++) {
			o = noOfBitsEmbedded;
			color[m] = data[j+k+m];
			for (n=0; n<8; n++) {
				colBin[n] = 0;
			}
			n = 0;
			while (color[m] > 0) {
				colBin[n] = color[m] % 2;
				color[m] /= 2;
				n++;
			}
			for (l=0; l<noOfLSB; l++) {
				if (o >= noOfBits) {
					break;
				}
				colBin[l] = secretBits[o++];
			}
			color[m] = bin2dec(colBin);

			if(((j+k+m-3)>0) && ((j+k+m+3)<noOfPixels))
			{
				diff[m] = abs(data[j+k+m-3] - color[m]);
				diff[m] += abs(data[j+k+m+3] - color[m]);
			}
			else {
				flag = 1;
			}
		}
		
		if(flag == 0)
		{
			m = getmin(diff[0], diff[1], diff[2]);
			m = 2;
		}
		else {
			m = 1;
			flag = 0;
		}

		data[j+k+m] = static_cast <unsigned char>(color[m]);
		noOfBitsEmbedded = o;
		
		i++;		
	}
	
	
	//add noOfBits to ithkey
	header = key[i];
	j = (header/step)*step;
	k = (header%step)*nChannels;
	l = noOfBits / 255;
	
	for (m=0; m<=l; m++) {
		if (m == l) {
			data[j+k+m] = static_cast <unsigned char>(noOfBitsEmbedded);
			noOfBitsEmbedded = 0;
		}
		else {
			data[j+k+m] = static_cast <unsigned char>(255);
			noOfBitsEmbedded -= 255; 
		}
	}
	
	writeKey();
	writeEmbeddedImage(img);
	
	
	return img;
}

void ExtractFiles()
{
	int k=embeddedFileName.find_last_of("_");	
	keyFileName = embeddedFileName.substr(0, k) + "_KEY.txt";
}


void ExtractKey()
{
	int i = 0;
	key = (int *)malloc(noOfPixels*sizeof(int));
	ifstream keyIF;
	keyIF.open(string2constChar(keyFileName));
	while ( !keyIF.eof() )
	{
		keyIF >> key[i++];
	}
	i-=2;
	header = key[i];
	keyIF.close();
}


void ExtractMessage()
{
	int i, n;
	char *ch = (char *)malloc((noOfBits/8) * sizeof(char));
	
	int decValue;
	for (i=0; i<noOfBits/8; i++) {
		decValue = 0;
		for (n=0; n<8; n++) {
			decValue += secretBits[i*8 + n] * pow(2, n);
		}
		ch[i] = (char)decValue;
	}
	
	
	getSecretMsg(ch);
}


void CarrierEngineExtract ( IplImage* img )
{
	int width = img->width, height = img->height, step = img->widthStep;
	nChannels = img->nChannels;
	int i, j, k, l, m, n, colBin[8], color[3], diff[3];
	bool flag = 0;
	
	noOfBitsEmbedded = 0;
	noOfPixels = width*height;
	noOfBits = 0;
	
	ExtractFiles();
	ExtractKey();
	
	uchar *data = ( uchar* )img->imageData;
	j = (header/step)*step;
	k = (header%step)*nChannels;
	m = 0;

	do {
		l = data[j+k+m];
		noOfBits += l;
		m++;
	} while (l == 255);
	
	
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	i = 0;
	while (noOfBitsEmbedded < noOfBits) {
		//extract data from key[i]th pixel
		j = (key[i]/step)*step;
		k = (key[i]%step)*nChannels;
		
		//extract bits	
		for (m=0; m<nChannels; m++) {
			if(((j+k+m-3)>0) && ((j+k+m+3)<noOfPixels))
			{
				diff[m] = abs(data[j+k+m-3] - data[j+k+m]);
				diff[m] += abs(data[j+k+m+3] - data[j+k+m]);
			}
			else {
				flag = 1;
			}
		}
		
		if(flag == 0)
		{
			m = getmin(diff[0], diff[1], diff[2]);
			m = 2;
		}
		else {
			m = 1;
			flag = 0;
		}
		
		color[m] = data[j+k+m];

		for (n=0; n<8; n++) {
			colBin[n] = 0;
		}
		n = 0;
		while (color[m] > 0) {
			colBin[n] = color[m] % 2;
			color[m] /= 2;
			n++;
		}
		for (l=0; l<noOfLSB; l++) {
			if (noOfBitsEmbedded >= noOfBits) {
				break;
			}
			secretBits[noOfBitsEmbedded++] = colBin[l];
		}
		i++;		
	}
	
	ExtractMessage();	
}


void cvShowManyImages(char* title, int nArgs, ...) {
	
    // img - Used for getting the arguments 
    IplImage *img;
	
    // DispImage - the image in which input images are to be copied
    IplImage *DispImage;
	
    int size;
    int i;
    int m, n;
    int x, y;
	
    // w - Maximum number of images in a row 
    // h - Maximum number of images in a column 
    int w, h;
	
    // scale - How much we have to resize the image
    float scale;
    int max;
	
    // If the number of arguments is lesser than 0 or greater than 12
    // return without displaying 
    if(nArgs <= 0) {
        printf("Number of arguments too small....\n");
        return;
    }
    else if(nArgs > 12) {
        printf("Number of arguments too large....\n");
        return;
    }
    // Determine the size of the image, 
    // and the number of rows/cols 
    // from number of arguments 
    else if (nArgs == 1) {
        w = h = 1;
        size = 350;
    }
    else if (nArgs == 2) {
        w = 2; h = 1;
        size = 350;
    }
	else if (nArgs == 3) {
        w = 3; h = 1;
        size = 350;
    }
    else if (nArgs == 4) {
        w = 2; h = 2;
        size = 350;
    }
    else if (nArgs == 5 || nArgs == 6) {
        w = 3; h = 2;
        size = 200;
    }
    else if (nArgs == 7 || nArgs == 8) {
        w = 4; h = 2;
        size = 200;
    }
    else {
        w = 4; h = 3;
        size = 125;
    }
	
    // Create a new 3 channel image
	
    DispImage = cvCreateImage( cvSize(100 + size*w, 60 + size*h), 8, 3 );
	
    // Used to get the arguments passed
    va_list args;
    va_start(args, nArgs);
	
    // Loop for nArgs number of arguments
    for (i = 0, m = 20, n = 20; i < nArgs; i++, m += (20 + size)) {
		
        // Get the Pointer to the IplImage
        img = va_arg(args, IplImage*);
		
        // Check whether it is NULL or not
        // If it is NULL, release the image, and return
        if(img == 0) {
            printf("Invalid arguments");
            cvReleaseImage(&DispImage);
            return;
        }
		
        // Find the width and height of the image
        x = img->width;
        y = img->height;
		
        // Find whether height or width is greater in order to resize the image
        max = (x > y)? x: y;
		
        // Find the scaling factor to resize the image
        scale = (float) ( (float) max / size );
		
        // Used to Align the images
        if( i % w == 0 && m!= 20) {
            m = 20;
            n+= 20 + size;
        }
		
        // Set the image ROI to display the current image
        cvSetImageROI(DispImage, cvRect(m, n, (int)( x/scale ), (int)( y/scale )));
		
        // Resize the input image and copy the it to the Single Big Image
        cvResize(img, DispImage);
		
        // Reset the ROI in order to display the next image
        cvResetImageROI(DispImage);
    }
	
    // Create a new window, and show the Single Big Image
    cvNamedWindow( title, 1 );
    cvShowImage( title, DispImage);
	
    cvWaitKey();
    cvDestroyWindow(title);
	
    // End the number of arguments
    va_end(args);
	
    // Release the Image Memory
    cvReleaseImage(&DispImage);
}


void drawHistogram()
{
	IplImage* channel = cvCreateImage( cvGetSize(orgImg), 8, 1 );
	IplImage *hist_img_red = cvCreateImage(cvSize(850,500), 8, 3) ;
	IplImage *hist_img_green = cvCreateImage(cvSize(850,500), 8, 3) ;
	IplImage *hist_img_blue = cvCreateImage(cvSize(850,500), 8, 3) ;
	
	int i;
	cvSet( hist_img_red, cvScalarAll(255), 0 );
	cvSet( hist_img_green, cvScalarAll(255), 0 );
	cvSet( hist_img_blue, cvScalarAll(255), 0 );
	
	CvHistogram *hist_red[2];
	CvHistogram *hist_green[2];
	CvHistogram *hist_blue[2];
	
    int hist_size = 256;      
    float range[]={0,256};
    float* ranges[] = { range };
	
    float max_value = 0.0;
    float max = 0.0;
    float w_scale = 0.0;
	
	/* Create a 1-D Arrays to hold the histograms */
	for (i=0; i<2; i++) {
		hist_red[i] = cvCreateHist(1, &hist_size, CV_HIST_ARRAY, ranges, 1);
		hist_green[i] = cvCreateHist(1, &hist_size, CV_HIST_ARRAY, ranges, 1);
		hist_blue[i] = cvCreateHist(1, &hist_size, CV_HIST_ARRAY, ranges, 1);		
	}
	
	
	/* Red channel */
	cvSetImageCOI(orgImg,3);
	cvCopy(orgImg,channel);
	cvResetImageROI(orgImg);
	cvCalcHist( &channel, hist_red[0], 0, NULL );
	/* Green channel */
	cvSetImageCOI(orgImg,2);
	cvCopy(orgImg,channel);
	cvResetImageROI(orgImg);
	cvCalcHist( &channel, hist_green[0], 0, NULL );
	/* Blue channel */
	cvSetImageCOI(orgImg,1);
	cvCopy(orgImg,channel);
	cvResetImageROI(orgImg);
	cvCalcHist( &channel, hist_blue[0], 0, NULL );
	
	
	/* Red channel */
	cvSetImageCOI(embImg,3);
	cvCopy(embImg,channel);
	cvResetImageROI(embImg);
	cvCalcHist( &channel, hist_red[1], 0, NULL );
	/* Green channel */
	cvSetImageCOI(embImg,2);
	cvCopy(embImg,channel);
	cvResetImageROI(embImg);
	cvCalcHist( &channel, hist_green[1], 0, NULL );
	/* Blue channel */
	cvSetImageCOI(embImg,1);
	cvCopy(embImg,channel);
	cvResetImageROI(embImg);
	cvCalcHist( &channel, hist_blue[1], 0, NULL );
	
	
	
	/* Find the minimum and maximum values of the histograms */
	cvGetMinMaxHistValue( hist_red[0], 0, &max_value, 0, 0 );
	cvGetMinMaxHistValue( hist_red[1], 0, &max, 0, 0 );
	max_value = (max > max_value) ? max : max_value;
	cvScale( hist_red[0]->bins, hist_red[0]->bins, ((float)hist_img_red->height)/max_value, 0 );
	cvScale( hist_red[1]->bins, hist_red[1]->bins, ((float)hist_img_red->height)/max_value, 0 );
	
	cvGetMinMaxHistValue( hist_green[0], 0, &max_value, 0, 0 );
	cvGetMinMaxHistValue( hist_green[1], 0, &max, 0, 0 );
	max_value = (max > max_value) ? max : max_value;
	cvScale( hist_green[0]->bins, hist_green[0]->bins, ((float)hist_img_green->height)/max_value, 0 );
	cvScale( hist_green[1]->bins, hist_green[1]->bins, ((float)hist_img_green->height)/max_value, 0 );
	
	cvGetMinMaxHistValue( hist_blue[0], 0, &max_value, 0, 0 );
	cvGetMinMaxHistValue( hist_blue[1], 0, &max, 0, 0 );
	max_value = (max > max_value) ? max : max_value;
	cvScale( hist_blue[0]->bins, hist_blue[0]->bins, ((float)hist_img_blue->height)/max_value, 0 );
	cvScale( hist_blue[1]->bins, hist_blue[1]->bins, ((float)hist_img_blue->height)/max_value, 0 );
	
	
    /* Scale/Squeeze the histogram range to image width */
    w_scale = ((float)hist_img_red->width)/hist_size;

	/* Plot the Histograms */
    for( int i = 0; i < hist_size; i++ )
    {
		cvRectangle( hist_img_red, cvPoint((int)i*w_scale , hist_img_red->height), cvPoint((int)(i+1)*w_scale, hist_img_red->height - cvRound(cvGetReal1D(hist_red[0]->bins,i))), CV_RGB(0,0,0), -1, 0, 0 );
		cvRectangle( hist_img_green, cvPoint((int)i*w_scale , hist_img_green->height), cvPoint((int)(i+1)*w_scale, hist_img_green->height - cvRound(cvGetReal1D(hist_green[0]->bins,i))), CV_RGB(0,0,0), -1, 0, 0 );
		cvRectangle( hist_img_blue, cvPoint((int)i*w_scale , hist_img_blue->height), cvPoint((int)(i+1)*w_scale, hist_img_blue->height - cvRound(cvGetReal1D(hist_blue[0]->bins,i))), CV_RGB(0,0,0), -1, 0, 0 );
		
		cvRectangle( hist_img_red, cvPoint((int)i*w_scale , hist_img_red->height), cvPoint((int)(i+1)*w_scale, hist_img_red->height - cvRound(cvGetReal1D(hist_red[1]->bins,i))), CV_RGB(255,0,0), 0, 0, 0 );
		cvRectangle( hist_img_green, cvPoint((int)i*w_scale , hist_img_green->height), cvPoint((int)(i+1)*w_scale, hist_img_green->height - cvRound(cvGetReal1D(hist_green[1]->bins,i))), CV_RGB(0,255,0), 0, 0, 0 );
		cvRectangle( hist_img_blue, cvPoint((int)i*w_scale , hist_img_blue->height), cvPoint((int)(i+1)*w_scale, hist_img_blue->height - cvRound(cvGetReal1D(hist_blue[1]->bins,i))), CV_RGB(0,0,255), 0, 0, 0 );
    }
	
    /* create a window to show the histogram of the image */
	cvNamedWindow( "red", 1 );
    cvShowImage( "red", hist_img_red);
	
    cvWaitKey();
    cvDestroyWindow("red");
	cvNamedWindow( "green", 1 );
    cvShowImage( "green", hist_img_green);
	
    cvWaitKey();
    cvDestroyWindow("green");
	cvNamedWindow( "blue", 1 );
    cvShowImage(  "blue", hist_img_blue);
	
    cvWaitKey();
    cvDestroyWindow( "blue");
}

void statAnalysis()
{
	int width = embImg->width, height = embImg->height, step = embImg->widthStep;
	nChannels = embImg->nChannels;
	int i, j, k, dif[nChannels], t;
	float MSE[nChannels];
	float PSNR[nChannels];
	
	uchar *data = ( uchar* )orgImg->imageData;
	uchar *data1 = ( uchar* )embImg->imageData;
	
	dif[0] = 0;
	dif[1] = 0;
	dif[2] = 0;
	
	for( i = 0 ; i < height ; i++ ) {
		for( j = 0 ; j < width ; j++ ) {
			for (k=0; k<nChannels; k++) {
				t = data1[i*step+j*nChannels+k] - data[i*step+j*nChannels+k];
				dif[k] += (int)pow(t, 2);
			}
		}
	}
	
	cout << "\nMSE:\n";
	for (k=0; k<nChannels; k++) {
		MSE[k] = (float)dif[k] / (width*height);
		cout << MSE[k] << "\t";
	}
	
	
	cout << "\nPSNR:\n";
	for (i=0; i<nChannels; i++) {
		PSNR[i] = 20 * log10f(255 / sqrt(MSE[i]));
		cout << PSNR[i] << "\t";
	}
}

void stegAnalysis()
{
	cout << "\nTime take:" << (float)(stop - start)/CLOCKS_PER_SEC << " s\n";
	
	drawHistogram();
	statAnalysis();
}

int main (int argc, char * const argv[]) {
    // insert code here...
	start = clock();
	
	carrierFileName = "/Users/dasaanand/IS/BMP/Image5.bmp";
	orgImg = cvLoadImage(string2constChar(carrierFileName), CV_LOAD_IMAGE_COLOR);
	coverImg = cvLoadImage(string2constChar(carrierFileName), CV_LOAD_IMAGE_COLOR);
	
	embImg = CarrierEngine(coverImg);
	
	/*embeddedFileName = "/Users/dasaanand/IS/LSBTool/Image1.bmp";
	 embImg = cvLoadImage(string2constChar(embeddedFileName), CV_LOAD_IMAGE_COLOR);
	 */
	
	CarrierEngineExtract(embImg);
	stop = clock();
	
	cvShowManyImages(windowName, 2, orgImg, embImg );
	stegAnalysis();
	
    return 0;
}