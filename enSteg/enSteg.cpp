#include <OpenCV/OpenCV.h>
#include <OpenCV/cv.h>
#include <OpenCv/highgui.h>
#include <OpenCV/cv.h>
#include <openssl/des.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <iterator>
#include <omp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <Parameters.h>


using namespace std;


IplImage *coverImg, *orgImg, *embImg;
int *key,noOfBits, noOfBitsEmbedded = 0, header, noOfPixels;
bool *secretBits;
string carrierFileName, secretMsgFileName, keyFileName[3], embeddedFileName, encryptedFileName;
char * secretMsg, SB[32];
struct passwd *pw = getpwuid(getuid());
string homedir = pw->pw_dir;
clock_t start, stop;


class colors {
public:
	int Red, Green, Blue;
};

class coordinate {
public:
	int x, y;
};


template <typename Iterator>
Iterator compress(const string &uncompressed, Iterator result) {
	// Build the dictionary.
	int dictSize = 256;
	map<string,int> dictionary;
	for (int i = 0; i < 256; i++)
		dictionary[string(1, i)] = i;
	
	string w;
	for (string::const_iterator it = uncompressed.begin();
		 it != uncompressed.end(); ++it) {
		char c = *it;
		string wc = w + c;
		if (dictionary.count(wc))
			w = wc;
		else {
			*result++ = dictionary[w];
			// Add wc to the dictionary.
			dictionary[wc] = dictSize++;
			w = string(1, c);
		}
	}
	
	// Output the code for w.
	if (!w.empty())
		*result++ = dictionary[w];
	return result;
}

template <typename Iterator>
std::string decompress(Iterator begin, Iterator end) {
	// Build the dictionary.
	int dictSize = 256;
	std::map<int,std::string> dictionary;
	for (int i = 0; i < 256; i++)
		dictionary[i] = std::string(1, i);
	
	std::string w(1, *begin++);
	std::string result = w;
	std::string entry;
	for ( ; begin != end; begin++) {
		int k = *begin;
		if (dictionary.count(k))
			entry = dictionary[k];
		else if (k == dictSize)
			entry = w + w[0];
		else
			throw "Bad compressed k";
		
		result += entry;
		
		// Add w+entry[0] to the dictionary.
		dictionary[dictSize++] = w + entry[0];
		
		w = entry;
	}
	return result;
}

string int_to_str(int i)
{
    string s;
    stringstream out;
    out << i;
    s = out.str();	
    return s;
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

void writeKey(int col)
{
	int n = ceilf((float)noOfBits/(nChannels*noOfLSB)) + 1, i;
	ofstream keyOF;
	keyOF.open(string2constChar(keyFileName[col]));
	
	for (i=0; i<n; i++) {
		keyOF << key[i] << "\t";
	}
	keyOF.close();
}


void writeEmbeddedImage(IplImage* img)
{
	if(!cvSaveImage(string2constChar(embeddedFileName), img))
	{
		cout << "Cant save the file";
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
	secretMsg = "secret message";
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
	string path = homedir + "/IS/enStegTool";
	char * dir = string2constChar(path);
	mkdir(dir, 0777);
	
	
	embeddedFileName = path + "/";
	j=carrierFileName.find_last_of("/\\");
	j++;
	
	k=carrierFileName.find_last_of(".");
	
	embeddedFileName += carrierFileName.substr(j, k-j);
	keyFileName[0] = embeddedFileName + "_KEY1.txt";
	keyFileName[1] = embeddedFileName + "_KEY2.txt";
	keyFileName[2] = embeddedFileName + "_KEY3.txt";
	encryptedFileName = embeddedFileName + "_ENC.txt";

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


void ExtractKey(int col)
{
	int i = 0;
	key = (int *)malloc(noOfPixels*sizeof(int));
	ifstream keyIF;
	keyIF.open(string2constChar(keyFileName[col]));
	while ( !keyIF.eof() )
	{
		keyIF >> key[i++];
	}
	i-=2;
	header = key[i];
	keyIF.close();
}


char* ExtractMessage()
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
	
	return ch;
}


char *Encrypt( char *Key, char *Msg, int size)
{
	
	static char*    Res;
	int             n=0;
	DES_cblock      Key2;
	DES_key_schedule schedule;
	
	Res = ( char * ) malloc( size );
	
	/* Prepare the key for use with DES_cfb64_encrypt */
	memcpy( Key2, Key,8);
	DES_set_odd_parity( &Key2 );
	DES_set_key_checked( &Key2, &schedule );
	
	/* Encryption occurs here */
	DES_cfb64_encrypt( ( unsigned char * ) Msg, ( unsigned char * ) Res, size, &schedule, &Key2, &n, DES_ENCRYPT );
	
	return (Res);
}


char *Decrypt( char *Key, char *Msg, int size)
{
	
	static char*    Res;
	int             n=0;
	
	DES_cblock      Key2;
	DES_key_schedule schedule;
	
	Res = ( char * ) malloc( size );
	
	/* Prepare the key for use with DES_cfb64_encrypt */
	memcpy( Key2, Key,8);
	DES_set_odd_parity( &Key2 );
	DES_set_key_checked( &Key2, &schedule );
	
	/* Decryption occurs here */
	DES_cfb64_encrypt( ( unsigned char * ) Msg, ( unsigned char * ) Res, size, &schedule, &Key2, &n, DES_DECRYPT );
	
	return (Res);
	
}

int distance1(int R,int G,int B,int Rpx,int Gpx ,int Bpx)
{
	return (((R-Rpx)*(R-Rpx)) + ((G-Gpx)*(G-Gpx)) + ((B-Bpx)*(B-Bpx)));
}

IplImage * KMean(IplImage *Img , int Points)
{
	int height = Img->height;
	int dist=INT_MAX;
	int width = Img->width;
	int step = Img->widthStep;
	int channels = Img->nChannels;
	uchar *data;
	data = (uchar *) Img->imageData;
	
	vector <colors> Mean;
	vector < vector < coordinate > > v(Points);
	
	colors c;
	
	for(int i=0;i<Points;i++){
		int x ,y ;
		x = rand()%width;
		y = rand()%height;
		c.Red = data[ x* step + y *channels + 0],
		c.Green = data[ x* step + y *channels + 1],
		c.Blue = data[ x* step + y *channels + 2],
		Mean.push_back(c);
	}
	
	int R,G,B;
	omp_set_num_threads(200);
	while(dist){
		
		for(int i=0;i<height;i++){
			for(int j=0;j<width;j++){
				R = data[i*step+j*channels + 0];
				G = data[i*step+j*channels + 1];
				B = data[i*step+j*channels + 2];
				int Class = 0;
				int Min_distance = distance1(R,G,B,Mean[0].Red,Mean[0].Green,Mean[0].Blue);
				
				for(int l=1;l<Mean.size();l++){
					int dist = distance1(R,G,B,Mean[l].Red,Mean[l].Green,Mean[l].Blue);
					if(dist < Min_distance){
						Class = l;
						Min_distance = dist;
					}
					coordinate tmp;
					tmp.x = i;
					tmp.y = j;
					v[Class].push_back(tmp);
				}
			}
		}
		dist =0;
		
#pragma omp parallel for schedule(dynamic)
		for(int i=0;i<Points;i++){
			
			int AvgRed = 0;
			int AvgGreen = 0;
			int AvgBlue = 0;
			
#pragma omp parallel for schedule(dynamic)
			for(int j=0;j<v[i].size();j++){
				AvgRed = AvgRed +
				data[v[i][j].x*step+v[i][j].y*channels+0];
				AvgGreen = AvgGreen +
				data[v[i][j].x*step+v[i][j].y*channels+1];
				AvgBlue = AvgBlue +
				data[v[i][j].x*step+v[i][j].y*channels+2];
				
			}
			if (v[i].size() != 0) {
				dist = dist + distance1(Mean[i].Red ,Mean[i].Green,Mean[i].Blue,AvgRed/v[i].size(),AvgGreen/v[i].size(),AvgBlue/v[i].size());
				Mean[i].Red = AvgRed / v[i].size();
				Mean[i].Green = AvgGreen / v[i].size();
				Mean[i].Blue = AvgBlue / v[i].size();
				
			}
			
			if(dist){
				v[i].clear();
			}
			
		}
		//cout << dist << " "<< endl;	
	}
	
#pragma omp parallel for schedule(dynamic)
	for(int i=0;i<Points;i++){
		for(int j=0;j<v[i].size();j++){
			data[v[i][j].x*step+v[i][j].y*channels+0] = Mean[i].Red;
			data[v[i][j].x*step+v[i][j].y*channels+1] = Mean[i].Green;
			data[v[i][j].x*step+v[i][j].y*channels+2] = Mean[i].Blue;
		}
		v[i].clear();
	}
	
	return Img;
	
}

IplImage* EA(IplImage* img, string SM, int colo)
{
	int TM, width=img->width, height=img->height, step=img->widthStep;
	int i, j, k, l, m, n, col, colBin[8];
	nChannels = img->nChannels;
	
	TM = SM.length() * 8;

	const char *sc1 = string2constChar(SM);
	noOfBits = TM;
	noOfBitsEmbedded = 0;
	secretBits = (bool *)malloc(noOfBits*sizeof(bool));	
	
	for (i=0; i<noOfBits; i++) {
		secretBits[i] = 0;
	}
	
	noOfBitsEmbedded = 0;
	for (i=0; i<SM.length(); i++) {
		dec2bin((int)sc1[i]);
		noOfBitsEmbedded = (i+1)*8;
	}
	
	noOfBitsEmbedded=0;
	noOfPixels = width*height;
	GenerateKey();
	
	uchar *data = ( uchar* )img->imageData;
	
	i = 0;
	while (noOfBitsEmbedded < noOfBits) {
		//extract data from key[i]th pixel
		j = (key[i]/step)*step;
		k = (key[i]%step)*nChannels;
		
		//embedd bits	
		for (m=0; m<nChannels; m++) {
			col = data[j+k+m];
			for (n=0; n<8; n++) {
				colBin[n] = 0;
			}
			n = 0;
			while (col > 0) {
				colBin[n] = col % 2;
				col /= 2;
				n++;
			}
			for (l=0; l<noOfLSB; l++) {
				if (noOfBitsEmbedded >= noOfBits) {
					break;
				}
				colBin[l] = secretBits[noOfBitsEmbedded++];
			}
			data[j+k+m] = static_cast <unsigned char>(bin2dec(colBin));
		}
		
		//add 6 to noofBitsEmbedded
		i++;
		
	}
	
	//add noOfBits to ithkey
	header = key[i];
	j = (header/step)*step;
	k = (header%step)*nChannels;
	l = noOfBits / 255;
	//noOfBitsEmbedded--;
	
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

	writeKey(colo);
	return img;
}


char* IEA(IplImage* img, int colo)
{
	int width = img->width, height = img->height, step = img->widthStep;
	nChannels = img->nChannels;
	int i, j, k, l, m, n, col, colBin[8];
	
	noOfPixels = width * height;
	noOfBitsEmbedded = 0;
	noOfBits = 0;

	ExtractKey(colo);
	
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
		
		//embedd bits
		for (m=0; m<nChannels; m++) {
			col = data[j+k+m];
			for (n=0; n<8; n++) {
				colBin[n] = 0;
			}
			n = 0;
			while (col > 0) {
				colBin[n] = col % 2;
				col /= 2;
				n++;
			}
			for (l=0; l<noOfLSB; l++) {
				secretBits[noOfBitsEmbedded++] = colBin[l];
				if (noOfBitsEmbedded > noOfBits) {
					break;
				}
			}
		}
		
		i++;		
	}
	
	return ExtractMessage();
}



IplImage* CarrierEngine ( IplImage* img )
{
	uint16_t l1, l2;
	int TM, TP, width=img->width, height=img->height;
	int i, j, k, count;
	
	char *encrypted[3];
	string si3[3], SM;
	vector<int> compressed;

	IplImage* RImg = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* GImg = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* BImg = cvCreateImage(cvSize(width, height), 8, 1);
	
	nChannels = img->nChannels;
	
	
	GenerateFiles();
	setSecretMsg();
	compress(secretMsg, back_inserter(compressed));
	copy(compressed.begin(), compressed.end(), ostream_iterator<int>(cout, ", "));
	count = compressed.size();
	cout << endl;
	
	l1 = lfsr1;
	l2 = lfsr2;
	i = 0;
	j = ceilf((float)count/3);
	int SI[3][j];
	
	do {
		/* taps: 8 6 5 4; characteristic polynomial: x^8 + x^6 + x^5 + x^4 + 1 */
		l1 = (l1 >> 1) ^ (-(l1 & 1u) & 0xB8u); 
		l2 = (l2 >> 1) ^ (-(l2 & 1u) & 0xB8u); 
	
		if (i >= j) {
			if (i >= 2*j) {
				k = 2;
			}
			else {
				k = 1;
			}
		}
		else {
			k = 0;
		}

		SI[k][i] = l1 ^ l2;
		SI[k][i] = SI[k][i] ^ compressed.at(i);
		si3[k] += int_to_str(SI[k][i]);
		si3[k] += " ";
		i++;
	} while(l1 != lfsr1 && l2 != lfsr2 && i<count);
	

	SM = "";
	for (i=0; i<3; i++) {
		char clear[si3[i].length()];
		strcpy(clear, string2constChar(si3[i]));
		
		encrypted[i] = (char *)malloc(sizeof(clear));
		memcpy(encrypted[i],Encrypt(DESkey,clear,sizeof(clear)), sizeof(clear));

		SM += encrypted[i];
	}
	
	
	TP = width * height;
	TM = SM.length() * 8;
	while (noOfLSB <= 4) {
		if (3*noOfLSB*TP >= TM) {
			break;
		}
		else {
			noOfLSB++;
		}
	}

	img = KMean(img, MAX_CLUSTERS);

	cvSplit(img, RImg, GImg, BImg, NULL);
	
	RImg = EA(RImg, si3[0], 2);
	GImg = EA(GImg, si3[1], 1);
	BImg = EA(BImg, si3[2], 0);
	
	cvZero(img);
	cvMerge(RImg, GImg, BImg, NULL, img);

	writeEmbeddedImage(img);

	return img;
}

void ExtractFiles()
{
	int k=embeddedFileName.find_last_of("_");	
	keyFileName[0] = embeddedFileName.substr(0, k) + "_KEY1.txt";
	keyFileName[1] = embeddedFileName.substr(0, k) + "_KEY2.txt";
	keyFileName[2] = embeddedFileName.substr(0, k) + "_KEY3.txt";
	encryptedFileName = embeddedFileName.substr(0, k) + "_ENC.txt";
}


void CarrierEngineExtract ( IplImage* img )
{
	uint16_t l1, l2;
	int width = img->width, height = img->height;
	int i, j, k, count;
	char *ch[3];
	vector<int> compressed;

	nChannels = img->nChannels;
	IplImage* RImg = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* GImg = cvCreateImage(cvSize(width, height), 8, 1);
	IplImage* BImg = cvCreateImage(cvSize(width, height), 8, 1);
	
	
	
	cout << "\n---DECRYPTING----- \n";
	nChannels = img->nChannels;
	
	ExtractFiles();
	
	cvSetImageCOI(img,3);
	cvSplit(img, RImg, GImg, BImg, NULL);


	ch[0] = IEA(RImg, 2);
	ch[1] = IEA(GImg, 1);
	ch[2] = IEA(BImg, 0);
	
	
	for (i=0; i<3; i++) {
		j = 0;
		k = 0;
		while (ch[i][j] != '\0') {
			if (ch[i][j] != ' ') {
				k *= 10;
				k += int(ch[i][j]) - 48;
			}
			else {
				cout << "\n" << k;
				compressed.push_back(k);
				k = 0;
			}
			j++;
		}
	}
	
	
	
	count = compressed.size();
	cout << "Count:" << count;

	l1 = lfsr1;
	l2 = lfsr2;
	i = 0;
	j = ceilf((float)count/3);
	int SI[3][j];
	
	do {
		/* taps: 8 6 5 4; characteristic polynomial: x^8 + x^6 + x^5 + x^4 + 1 */
		l1 = (l1 >> 1) ^ (-(l1 & 1u) & 0xB8u); 
		l2 = (l2 >> 1) ^ (-(l2 & 1u) & 0xB8u); 
		
		if (i >= j) {
			if (i >= 2*j) {
				k = 2;
			}
			else {
				k = 1;
			}
		}
		else {
			k = 0;
		}
		
		SI[k][i] = l1 ^ l2;
		compressed.at(i) = SI[k][i] ^ compressed.at(i);
		i++;
	} while(l1 != lfsr1 && l2 != lfsr2 && i<count);
	
	
	string decompressed = decompress(compressed.begin(), compressed.end());
	//cout << decompressed << endl;
	getSecretMsg(string2constChar(decompressed));
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