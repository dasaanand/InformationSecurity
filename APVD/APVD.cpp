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

#define getmax(a,b) a>b?a:b
#define getmin(a,b) a<b?a:b

using namespace std;

IplImage *coverImg, *orgImg, *embImg;
int *key1, *key2, noOfBits, noOfBitsEmbedded = 0, header, noOfEmbeddingUnits, noOfSquares;
bool *secretBits;
string carrierFileName, secretMsgFileName, key1FileName, key2FileName, embeddedFileName;
char * secretMsg;
clock_t t1, t2;

struct passwd *pw = getpwuid(getuid());
string homedir = pw->pw_dir;
clock_t start, stop;


int Resize(int n, int m, int l)             //Gets a number between the range specified
{
    if(n<=l && l<=m)
		return l;
    else if(l<n)
		return Resize(n, m, l*2);		//if l<n then double the value
    else
		return Resize(n, m, l/3);		//if not l = l/3;
    t2 = clock();
}

int Random(int n, int m)                   //Generates the random number using time.
{
    t1 = clock();
	unsigned int x = x*x*(t2-t1)*10;
    
	int no = Resize(n, m, (1+x%10));	//reducing the random no within the given range
	
	switch (no) {
		case 1:
			return 0;
			break;
		case 2:
			return 90;
			break;
		case 3:
			return 180;
			break;
		default:
			return 270;
			break;
	}
    
}

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


void writeKeys(int k)
{
	int i;
	ofstream key1OF, key2OF;
	key1OF.open(string2constChar(key1FileName));
	for (i=0; i<noOfSquares; i++) {
		key1OF << key1[i] << "\t";
	}
	key1OF.close();
	
	key2OF.open(string2constChar(key2FileName));
	for (i=0; i<k; i++) {
		key2OF << key2[i] << "\t";
	}
	key2OF.close();
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
	string path = homedir + "/IS/APVDTool";
	char * dir = string2constChar(path);
	mkdir(dir, 0777);
	
	
	embeddedFileName = path + "/";
	j=carrierFileName.find_last_of("/\\");
	j++;
	
	k=carrierFileName.find_last_of(".");
	
	embeddedFileName += carrierFileName.substr(j, k-j);
	key1FileName = embeddedFileName + "_KEY1.txt";
	key2FileName = embeddedFileName + "_KEY2.txt";
	embeddedFileName += "_EMBED.";
	embeddedFileName += carrierFileName.substr(k+1, carrierFileName.length()-k);
}


void GenerateKey1()
{
	int i;
	key1 = (int *)malloc(noOfSquares*sizeof(int));
	
	omp_set_num_threads(200);
#pragma omp parallel for schedule(dynamic)
	for (i=0; i<noOfSquares; i++) {
		key1[i] = Random(1, 5);
	}
}

void GenerateKey2()
{
	int i, j, temp;
	key2 = (int *)malloc(noOfEmbeddingUnits*sizeof(int));	
	for (i=0; i<noOfEmbeddingUnits; i++) {
		key2[i] = i;
	}
	
	omp_set_num_threads(200);
#pragma omp parallel for schedule(dynamic)
	for (i=0; i<noOfEmbeddingUnits; i++) {
		j = rand()%(i+1);
		temp = key2[i];
		key2[i] = key2[j];
		key2[j] = temp;
	}
}


IplImage* ReRotateSquares (IplImage* img)
{
	int x=0, y=0, k, step = img->widthStep;		
	CvPoint2D32f center;
	
	
	for (k =0; k<noOfSquares; k++)
	{
		//rotate the squares
		/*center.x = x + Z/2; 
		center.y = y + Z/2;
		 
		cvSetImageROI(img, cvRect(x, y, Z, Z));	
		CvMat *N = cvCreateMat(Z/2, Z/2, CV_32F);
		cv2DRotationMatrix(center, 360-key1[k], 1 , N);
		cvWarpAffine(img, img, N, CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll(0));	
		cvResetImageROI(img);*/
		
		y += Z;
		if (y >=step) {
			x += Z;
			y = 0;
		}
		
	}
	return img;
}


IplImage* RotateSquares (IplImage* img)
{
	int x=0, y=0, k, step=img->widthStep;
	
	CvPoint2D32f center;
	
	for (k =0; k<noOfSquares; k++)
	{
		//rotate the squares
		center.x = x + Z/2; 
		center.y = y + Z/2;
		 
		/*cvSetImageROI(img, cvRect(x, y, Z, Z));	
		CvMat *N = cvCreateMat(Z/2, Z/2, CV_32F);
		cv2DRotationMatrix(center, key1[k], 1 , N);
		cvWarpAffine(img, img, N, CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll(0));	
		cvResetImageROI(img);*/
		
		y += Z;
		if (y >=step) {
			x += Z;
			y = 0;
		}
		
	}
	return img;
}


IplImage* CarrierEngine ( IplImage* img )
{
	int width = img->width, height = img->height; 
	int g[3], g1, d[2], i, k, l, m, lower, upper, n, e, b, minComp;
	
	
	noOfSquares = (width * height)/(Z * Z);
	noOfEmbeddingUnits = (width*height)/3;
	
	setSecretMsg();
	noOfBits = 8 * strlen(secretMsg);
	noOfBitsEmbedded = 0;
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	for (i=0; i<strlen(secretMsg); i++) {
		dec2bin((int)secretMsg[i]);
		noOfBitsEmbedded = (i+1)*8;
	}
	noOfBitsEmbedded=0;

	GenerateKey1();
	GenerateKey2();

	img = RotateSquares(img);
	uchar *data = ( uchar* )img->imageData;

	
	k = 0;
	while (noOfBitsEmbedded < noOfBits) {
		g[0] = data[key2[k] * 3];
		g[1] = data[key2[k] * 3 + 1];
		g[2] = data[key2[k] * 3 + 2];
		d[0] = g[1] - g[0];
		d[1] = g[2] - g[1];

		if((abs(d[0]) > T) || (abs(d[1]) > T))
		{
			if ((g[1] < g[0]) && (g[1] < g[2])) {	//1
				if ((abs(d[0]) > T) && (abs(d[1]) > T)) {	//1.1
					lower = 0; upper = getmin(g[0]-T-1, g[2]-T-1);
					//cout << "1.1\t";
				}
				else {
					if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//1.2
						lower = getmax(g[2]-T,0); upper = getmin(g[0]-T-1, g[2]-1);
						//cout << "1.2\t";
					}
					else {
						if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//1.3
							lower = getmax(g[0]-T,0); upper = getmin(g[2]-T-1, g[0]-1);
							//cout << "1.3\t";
						}
					}
				}
			}
			else {
				if ((g[1] > g[0]) && (g[1] > g[2])) {	//2
					if ((abs(d[0]) > T) && (abs(d[1]) > T)) {	//2.1
						lower = getmax(g[0]+T+1,g[2]+T+1); upper = 255;
						//cout << "2.1\t";
						
					}
					else {
						if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//2.2
							lower = getmax(g[0]+T+1,g[2]+1); upper = getmin(g[2]+T,255);
							//cout << "2.2\t";
							
						}
						else {
							if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//2.3
								lower = getmax(g[2]+T+1,g[0]+1); upper = getmin(g[0]+T,255);
								//cout << "2.3\t";
								
							}
						}
					}
				}
				else {
					if ((g[0] >= g[1]) && (g[1] >= g[2])) {	//3
						if ((abs(d[0]) > T) && (abs(d[1]) > T))	{	//3.1
							lower = g[2]+T+1; upper = g[0]-T-1;
							//cout << "3.1\t";
							
						}
						else {
							if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//3.2
								lower = g[2]; upper = getmin(g[2]+T,g[0]-T-1);
								//cout << "3.2\t";
								
							}
							else {
								if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//3.3
									lower = getmax(g[2]+T+1,g[0]-T); upper = g[0];
									//cout << "3.3\t";
									
								}
							}
						}
					}
					else {
						if ((g[0] <= g[1]) && (g[1] <= g[2])) {	//4
							if ((abs(d[0]) > T) && (abs(d[1]) > T)) {	//4.1
								lower = g[0]+T+1; upper = g[2]-T-1;
								//cout << "4.1\t";
								
							}
							else {
								if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//4.2
									lower = getmax(g[2]-T,g[0]+T+1); upper=g[2];
									//cout << "4.2\t";
									
								}
								else {
									if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//4.3
										lower = g[0]; upper = getmin(g[2]-T-1,g[0]+T);
										//cout << "4.3\t";
									}
								}
							}
						}			
					}
				}
			}
			
			n = getmin((int)log2(upper-lower+1), K);
			if (n > 0) {
				minComp = INT_MAX;

				b = 0;
				for (i=n; i>0; i--) {
					if (noOfBitsEmbedded >= noOfBits) {
						break;
					}
					b += secretBits[noOfBitsEmbedded++] * pow(2, i-1);
				}

				
				for (e=lower; e<=upper; e++) {
					if ((((abs(e-g[0])) % (int)pow(2, n)) == b) && (abs(e-g[1]) < minComp)) {
						minComp = abs(e - g[1]);
						g1 = e;
					}
				}

				data[key2[k] * 3 + 1] = static_cast <unsigned char>(g1);

			}
		}
		k++;
	}	
	
	
	//add noOfBits to ithkey
	header = key2[k++];
	i = header*3;
	l = noOfBits / 255;
	
	for (m=0; m<=l; m++) {
		if (m == l) {
			data[i+m] = static_cast <unsigned char>(noOfBitsEmbedded);
			noOfBitsEmbedded = 0;
		}
		else {
			data[i+m] = static_cast <unsigned char>(255);
			noOfBitsEmbedded -= 255; 
		}
	}
	
	
	img = ReRotateSquares(img);
	GenerateFiles();
	writeKeys(k);
	writeEmbeddedImage(img);
	
	return img;
}


void ExtractFiles()
{
	int k=embeddedFileName.find_last_of("_");	
	key1FileName = embeddedFileName.substr(0, k) + "_KEY1.txt";
	key2FileName = embeddedFileName.substr(0, k) + "_KEY2.txt";
}


void ExtractKey1 ()
{
	int i = 0;
	key1 = (int *)malloc(noOfSquares*sizeof(int));
	ifstream key1IF;
	key1IF.open(string2constChar(key1FileName));
	while ( !key1IF.eof() )
	{
		key1IF >> key1[i++];
	}
	key1IF.close();
}

void ExtractKey2 ()
{
	int i = 0;
	key2 = (int *)malloc(noOfEmbeddingUnits*sizeof(int));
	ifstream key2IF;
	key2IF.open(string2constChar(key2FileName));
	while ( !key2IF.eof() )
	{
		key2IF >> key2[i++];
	}
	i-=2;
	header = key2[i];
	key2IF.close();
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
	int g[3], d[2], i, j, k, l, m, lower, upper, n, b, temp;

	noOfBitsEmbedded = 0;
	noOfBits = 0;
	
	ExtractFiles();
	ExtractKey1();
	ExtractKey2();
	
	img = RotateSquares(img);
	uchar *data = ( uchar* )img->imageData;

	//cout << "Header:" << header << "\n";
	i = header*3;
	l = 0;
	m = 0;
	
	do {
		l = data[i+m];
		noOfBits += l;
		m++;
	} while (l == 255);
	
	//cout << noOfBits << "\n";
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	
	
	k = 0;
	while (noOfBitsEmbedded < noOfBits) {
		//cout << "\n" << key2[k] << "\t\t";
		g[0] = data[key2[k] * 3];
		g[1] = data[key2[k] * 3 + 1];
		g[2] = data[key2[k] * 3 + 2];
		d[0] = g[1] - g[0];
		d[1] = g[2] - g[1];
		
		//cout << g[0] << "\t" << g[1] << "\t" << g[2] << "\t\t";

		if((abs(d[0]) > T) || (abs(d[1]) > T))
		{
			if ((g[1] < g[0]) && (g[1] < g[2])) {	//1
				if ((abs(d[0]) > T) && (abs(d[1]) > T)) {	//1.1
					lower = 0; upper = getmin(g[0]-T-1, g[2]-T-1);
					//cout << "1.1\t";
				}
				else {
					if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//1.2
						lower = getmax(g[2]-T,0); upper = getmin(g[0]-T-1, g[2]-1);
						//cout << "1.2\t";
					}
					else {
						if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//1.3
							lower = getmax(g[0]-T,0); upper = getmin(g[2]-T-1, g[0]-1);
							//cout << "1.3\t";
						}
					}
				}
			}
			else {
				if ((g[1] > g[0]) && (g[1] > g[2])) {	//2
					if ((abs(d[0]) > T) && (abs(d[1]) > T)) {	//2.1
						lower = getmax(g[0]+T+1,g[2]+T+1); upper = 255;
						//cout << "2.1\t";
						
					}
					else {
						if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//2.2
							lower = getmax(g[0]+T+1,g[2]+1); upper = getmin(g[2]+T,255);
							//cout << "2.2\t";
							
						}
						else {
							if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//2.3
								lower = getmax(g[2]+T+1,g[0]+1); upper = getmin(g[0]+T,255);
								//cout << "2.3\t";
								
							}
						}
					}
				}
				else {
					if ((g[0] >= g[1]) && (g[1] >= g[2])) {	//3
						if ((abs(d[0]) > T) && (abs(d[1]) > T))	{	//3.1
							lower = g[2]+T+1; upper = g[0]-T-1;
							//cout << "3.1\t";
							
						}
						else {
							if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//3.2
								lower = g[2]; upper = getmin(g[2]+T,g[0]-T-1);
								//cout << "3.2\t";
								
							}
							else {
								if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//3.3
									lower = getmax(g[2]+T+1,g[0]-T); upper = g[0];
									//cout << "3.3\t";
									
								}
							}
						}
					}
					else {
						if ((g[0] <= g[1]) && (g[1] <= g[2])) {	//4
							if ((abs(d[0]) > T) && (abs(d[1]) > T)) {	//4.1
								lower = g[0]+T+1; upper = g[2]-T-1;
								//cout << "4.1\t";
								
							}
							else {
								if ((abs(d[0]) > T) && (abs(d[1]) <= T)) {	//4.2
									lower = getmax(g[2]-T,g[0]+T+1); upper=g[2];
									//cout << "4.2\t";
									
								}
								else {
									if ((abs(d[0]) <= T) && (abs(d[1]) > T)) {	//4.3
										lower = g[0]; upper = getmin(g[2]-T-1,g[0]+T);
										//cout << "4.3\t";
									}
								}
							}
						}			
					}
				}
			}
			
			n = getmin((int)log2(upper-lower+1), K);
			//cout << "\t\t" <<g[0] << "\t" << g[1] << "\t" << g[2] << "\t" << n << "\t";
			
			if (n > 0) {
				b = abs(g[1] - g[0]) % (int)pow(2, n);
				
				for (i=n; i>0 && noOfBitsEmbedded<noOfBits; i--) {
					secretBits[noOfBitsEmbedded++] = 0;
				}
				
				temp = noOfBitsEmbedded;
				while (b>0) {
					secretBits[--noOfBitsEmbedded] = b%2;
					b /=2;
					n--;
				}
				noOfBitsEmbedded = temp;
				
				//cout << "BITS:" << noOfBitsEmbedded;
			}
		}
		k++;
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
	
	carrierFileName = "/Users/dasaanand/IS/BMP/Image2.bmp";
	
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