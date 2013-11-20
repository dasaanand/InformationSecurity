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

using namespace std;

IplImage *coverImg, *orgImg, *embImg;
int *key, noOfBits, noOfBitsEmbedded = 0, noOfPixelBlocks = 0, noOfDataBursts = 0, header;
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


void writeKey(int n)
{
	int i;
	ofstream keyOF;
	keyOF.open(string2constChar(keyFileName));
	
	for (i=0; i<=n; i++) {
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
	string path = homedir + "/IS/CountingOutTool";
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
	key = (int *)malloc(noOfPixelBlocks*sizeof(int));	
	for (i=0; i<noOfPixelBlocks; i++) {
		key[i] = i;
	}
	
	omp_set_num_threads(200);
#pragma omp parallel for schedule(dynamic)
	for (i=0; i<noOfPixelBlocks; i++) {
		j = rand()%(i+1);
		temp = key[i];
		key[i] = key[j];
		key[j] = temp;
	}
}



IplImage* CarrierEngine ( IplImage* img )
{
	int width = img->width, height = img->height, step = img->widthStep;
	int a, i, j, k, l, m, n, o, index, count, color, colBin[8];
	bool emb[pixelBlockSize/2][pixelBlockSize/2], flag;
	
	noOfPixelBlocks = (width * height)/pixelBlockSize;
	SPBS = (int)sqrt(pixelBlockSize);
	
	setSecretMsg();
	noOfBits = 8 * strlen(secretMsg);
	noOfBitsEmbedded = 0;
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	noOfDataBursts = ceilf((float)noOfBits/noOfLSB) + 1;
	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	for (i=0; i<strlen(secretMsg); i++) {
		dec2bin((int)secretMsg[i]);
		noOfBitsEmbedded = (i+1)*8;
	}
	noOfBitsEmbedded=0;

	GenerateKey();
	GenerateFiles();

	uchar *data = ( uchar* )img->imageData;

	a = 0;
	while (noOfBitsEmbedded < noOfBits) {
		i = (key[a] / (step / SPBS)) * SPBS;
		j = (key[a] % (step / SPBS)) * SPBS;
		
		for (k = 0; k < SPBS; k++) {
			for (l = 0; l < SPBS; l++) {
				emb[k][l] = 0;
			}
		}
		k = 0;
		l = 0;
		index = 0;
		flag = 0;
		
		while ((index < pixelBlockSize) && (noOfBitsEmbedded < noOfBits)) {
			for (n=0; n<8; n++) {
				colBin[n] = 0;
			}
			
			color = data[i+j+k*step+l];
			n = 0;
			
			while (color > 0) {
				colBin[n] = color % 2;
				color /= 2;
				n++;
			}
			
			for (m = 0; m < noOfLSB; m++) {
				if (noOfBitsEmbedded >= noOfBits) {
					break;
				}
				colBin[m] = secretBits[noOfBitsEmbedded++];
			}
			
			color = bin2dec(colBin);
			
			data[i+j+k*step+l] = static_cast <unsigned char>(color);

			emb[k][l] = 1;
			index++;
			
			if (index < 16) {
				for (o = 7; o > 3; o--) {
					colBin[o] = 0;
				}
				count = bin2dec(colBin);
				
				for (; count >= 0; count--) {					
					if (flag == 0) {
						l++;
						if (l >= SPBS) {
							l--;
							flag = 1;
							k = (k+1) % SPBS;
						}
					}
					else {
						l--;
						if (l < 0) {
							l++;
							flag = 0;
							k = (k+1) % SPBS;
						}
					}
					
					
					if (emb[k][l] == 1) {
						count++;
					}
				}
			}
		}
		a++;
	}
	
	
	//add noOfBits to ithkey
	header = key[a];
	i = (header / (step / SPBS)) * SPBS;
	j = (header % (step / SPBS)) * SPBS;
	l = noOfBits / 255;
	
	for (m=0; m<=l; m++) {
		if (m == l) {
			data[i+j+m] = static_cast <unsigned char>(noOfBitsEmbedded);
			noOfBitsEmbedded = 0;
		}
		else {
			data[i+j+m] = static_cast <unsigned char>(255);
			noOfBitsEmbedded -= 255; 
		}
	}
	
	writeKey(a);
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
	free(key);
	key = (int *)malloc(noOfPixelBlocks*sizeof(int));
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
	int a, i, j, k, l, m, n, o, index, count, color, colBin[8];
	bool emb[pixelBlockSize/2][pixelBlockSize/2], flag;

	noOfBitsEmbedded = 0;
	noOfBits = 0;
	noOfPixelBlocks = (width*height) / (pixelBlockSize);
	SPBS = (int)sqrt(pixelBlockSize);
	
	ExtractFiles();
	ExtractKey();
	
	uchar *data = ( uchar* )img->imageData;
	
	i = (header / (step / SPBS)) * SPBS;
	j = (header % (step / SPBS)) * SPBS;
	l = noOfBits / 255;
	m = 0;
	
	do {
		l = data[i+j+m];
		noOfBits += l;
		m++;
	} while (l == 255);
	
	
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	a = 0;
	while (noOfBitsEmbedded < noOfBits) {
		i = (key[a] / (step / SPBS)) * SPBS;
		j = (key[a] % (step / SPBS)) * SPBS;
		
		for (k = 0; k < SPBS; k++) {
			for (l = 0; l < SPBS; l++) {
				emb[k][l] = 0;
			}
		}
		k = 0;
		l = 0;
		index = 0;
		flag = 0;
		
		while ((index < pixelBlockSize) && (noOfBitsEmbedded < noOfBits)) {
			for (n=0; n<8; n++) {
				colBin[n] = 0;
			}
			
			color = data[i+j+k*step+l];
			n = 0;
			
			while (color > 0) {
				colBin[n] = color % 2;
				color /= 2;
				n++;
			}
			
			for (m = 0; m < noOfLSB; m++) {
				if (noOfBitsEmbedded >= noOfBits) {
					break;
				}
				secretBits[noOfBitsEmbedded++] = colBin[m];
			}
			
			color = bin2dec(colBin);
			
			emb[k][l] = 1;
			index++;
			
			if (index < 16) {
				for (o = 7; o > 3; o--) {
					colBin[o] = 0;
				}
				count = bin2dec(colBin);
				
				for (; count >= 0; count--) {
					if (flag == 0) {
						l++;
						if (l >= SPBS) {
							l--;
							flag = 1;
							k = (k+1) % SPBS;
						}
					}
					else {
						l--;
						if (l < 0) {
							l++;
							flag = 0;
							k = (k+1) % SPBS;
						}
					}
					
					
					if (emb[k][l] == 1) {
						count++;
					}
				}
			}
		}
		a++;
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
	
    // Create a new 1 channel image
	
    DispImage = cvCreateImage( cvSize(100 + size*w, 60 + size*h), 8, 1 );
	
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
	IplImage *hist_img = cvCreateImage(cvSize(850,500), 8, 1) ;
	IplImage *hist_img1 = cvCreateImage(cvSize(850,500), 8, 1) ;
	
	int i;
	cvSet( hist_img, cvScalarAll(255), 0 );
	cvSet( hist_img1, cvScalarAll(255), 0 );
	
	CvHistogram *hist_gray[2];
	
    int hist_size = 256;      
    float range[]={0,256};
    float* ranges[] = { range };
	
    float max_value = 0.0;
    float max = 0.0;
    float w_scale = 0.0;
	
	/* Create a 1-D Arrays to hold the histograms */
	for (i=0; i<2; i++) {
		hist_gray[i] = cvCreateHist(1, &hist_size, CV_HIST_ARRAY, ranges, 1);
	}
	
	
	cvCalcHist( &channel, hist_gray[0], 0, NULL );
	cvCalcHist( &channel, hist_gray[1], 0, NULL );
	
	
	/* Find the minimum and maximum values of the histograms */
	cvGetMinMaxHistValue( hist_gray[0], 0, &max_value, 0, 0 );
	cvGetMinMaxHistValue( hist_gray[1], 0, &max, 0, 0 );
	max_value = (max > max_value) ? max : max_value;
	cvScale( hist_gray[0]->bins, hist_gray[0]->bins, ((float)hist_img->height)/max_value, 0 );
	cvScale( hist_gray[1]->bins, hist_gray[1]->bins, ((float)hist_img->height)/max_value, 0 );

	
    /* Scale/Squeeze the histogram range to image width */
    w_scale = ((float)hist_img->width)/hist_size;

	/* Plot the Histograms */
    for( int i = 0; i < hist_size; i++ )
    {
		cvRectangle( hist_img, cvPoint((int)i*w_scale , hist_img->height), cvPoint((int)(i+1)*w_scale, hist_img->height - cvRound(cvGetReal1D(hist_gray[0]->bins,i))), cvScalar(0), -1, 0, 0 );
		
		cvRectangle( hist_img1, cvPoint((int)i*w_scale , hist_img1->height), cvPoint((int)(i+1)*w_scale, hist_img1->height - cvRound(cvGetReal1D(hist_gray[1]->bins,i))), cvScalar(0), 0, 0, 0 );
	}
	
    /* create a window to show the histogram of the image */
	cvNamedWindow( "GrayScale Org", 1 );
    cvShowImage( "GrayScale Org", hist_img);

	cvNamedWindow( "GrayScale Emb", 1 );
    cvShowImage( "GrayScale Emb", hist_img);

    cvWaitKey();
    cvDestroyWindow("GrayScale Org");
	cvDestroyWindow("GrayScale Emb");
}

void statAnalysis()
{
	int width = embImg->width, height = embImg->height, step = embImg->widthStep;
	int nChannels = embImg->nChannels;
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
	
	orgImg = cvLoadImage(string2constChar(carrierFileName), CV_LOAD_IMAGE_GRAYSCALE);
	coverImg = cvLoadImage(string2constChar(carrierFileName), CV_LOAD_IMAGE_GRAYSCALE);
	
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