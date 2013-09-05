#include <opencv2/opencv.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include <time.h>

using namespace cv;
using namespace std;


/*
 * Convert training/testing set to be used by OpenCV Machine Learning algorithms.
 * TrainData is a matrix of size (#samples x max(#cols,#rows) per samples), in 32FC1.
 * Transposition of samples are made if needed.
 */
void convert_to_ml(const std::vector< cv::Mat > & train_samples, cv::Mat& trainData )
{
	//--Convert data
    const int rows = (int)train_samples.size();
    const int cols = (int)std::max( train_samples[0].cols, train_samples[0].rows );
    cv::Mat tmp(1, cols, CV_32FC1); //< used for transposition if needed
	trainData = cv::Mat(rows, cols, CV_32FC1 );
	auto& itr = train_samples.begin();
	auto& end = train_samples.end();
	for( int i = 0 ; itr != end ; ++itr, ++i )
	{
        CV_Assert( itr->cols == 1 ||
            itr->rows == 1 );
        if( itr->cols == 1 )
        {
            transpose( *(itr), tmp );
            tmp.copyTo( trainData.row( i ) );
        }
        else if( itr->rows == 1 )
        {
            itr->copyTo( trainData.row( i ) );
        }
	}
}

void load_images( const string & prefix, const string & filename, vector< Mat > & img_lst )
{
    string line;
    ifstream file;

    file.open( (prefix+filename).c_str() );
    if( !file.is_open() )
    {
        cerr << "Unable to open the list of images from " << filename << " filename." << endl;
        exit( -1 );
    }
    
    while( 1 )
    {
        getline( file, line );
        if( line == "" ) // no more file to read
            break;
        Mat img = imread( (prefix+line).c_str() ); // load the image
        if( !img.data ) // invalid image, just skip it.
            continue;
#ifdef _DEBUG
        imshow( "image", img );
        waitKey( 10 );
#endif
        img_lst.push_back( img.clone() );
    }
}

void sample_neg( const vector< Mat > & full_neg_lst, vector< Mat > & neg_lst, const Size & size )
{
    Rect box;
    box.width = size.width;
    box.height = size.height;

    const int size_x = box.width;
    const int size_y = box.height;

    srand( time( NULL ) );

    auto& img = full_neg_lst.begin();
    auto& end = full_neg_lst.end();
    for( ; img != end ; ++img )
    {
        box.x = rand() % (img->cols - size_x);
        box.y = rand() % (img->rows - size_y);
        Mat roi = (*img)(box);
        neg_lst.push_back( roi.clone() );
#ifdef _DEBUG
        imshow( "img", roi.clone() );
        waitKey( 10 );
#endif
    }
}

Mat get_hogdescriptor_visu(Mat& color_origImg, vector<float>& descriptorValues, const Size & size )
{   
    const int DIMX = size.width;
    const int DIMY = size.height;
    float zoomFac = 3;
    Mat visu;
    resize(color_origImg, visu, Size(color_origImg.cols*zoomFac, color_origImg.rows*zoomFac));
 
    int blockSize       = 16;
    int cellSize        = 8;
    int gradientBinSize = 9;
    float radRangeForOneBin = CV_PI/(float)gradientBinSize; // dividing 180� into 9 bins, how large (in rad) is one bin?
 
    // prepare data structure: 9 orientation / gradient strenghts for each cell
    int cells_in_x_dir = DIMX / cellSize;
    int cells_in_y_dir = DIMY / cellSize;
    int totalnrofcells = cells_in_x_dir * cells_in_y_dir;
    float*** gradientStrengths = new float**[cells_in_y_dir];
    int** cellUpdateCounter   = new int*[cells_in_y_dir];
    for (int y=0; y<cells_in_y_dir; y++)
    {
        gradientStrengths[y] = new float*[cells_in_x_dir];
        cellUpdateCounter[y] = new int[cells_in_x_dir];
        for (int x=0; x<cells_in_x_dir; x++)
        {
            gradientStrengths[y][x] = new float[gradientBinSize];
            cellUpdateCounter[y][x] = 0;
 
            for (int bin=0; bin<gradientBinSize; bin++)
                gradientStrengths[y][x][bin] = 0.0;
        }
    }
 
    // nr of blocks = nr of cells - 1
    // since there is a new block on each cell (overlapping blocks!) but the last one
    int blocks_in_x_dir = cells_in_x_dir - 1;
    int blocks_in_y_dir = cells_in_y_dir - 1;
 
    // compute gradient strengths per cell
    int descriptorDataIdx = 0;
    int cellx = 0;
    int celly = 0;
 
    for (int blockx=0; blockx<blocks_in_x_dir; blockx++)
    {
        for (int blocky=0; blocky<blocks_in_y_dir; blocky++)            
        {
            // 4 cells per block ...
            for (int cellNr=0; cellNr<4; cellNr++)
            {
                // compute corresponding cell nr
                int cellx = blockx;
                int celly = blocky;
                if (cellNr==1) celly++;
                if (cellNr==2) cellx++;
                if (cellNr==3)
                {
                    cellx++;
                    celly++;
                }
 
                for (int bin=0; bin<gradientBinSize; bin++)
                {
                    float gradientStrength = descriptorValues[ descriptorDataIdx ];
                    descriptorDataIdx++;
 
                    gradientStrengths[celly][cellx][bin] += gradientStrength;
 
                } // for (all bins)
 
 
                // note: overlapping blocks lead to multiple updates of this sum!
                // we therefore keep track how often a cell was updated,
                // to compute average gradient strengths
                cellUpdateCounter[celly][cellx]++;
 
            } // for (all cells)
 
 
        } // for (all block x pos)
    } // for (all block y pos)
 
 
    // compute average gradient strengths
    for (int celly=0; celly<cells_in_y_dir; celly++)
    {
        for (int cellx=0; cellx<cells_in_x_dir; cellx++)
        {
 
            float NrUpdatesForThisCell = (float)cellUpdateCounter[celly][cellx];
 
            // compute average gradient strenghts for each gradient bin direction
            for (int bin=0; bin<gradientBinSize; bin++)
            {
                gradientStrengths[celly][cellx][bin] /= NrUpdatesForThisCell;
            }
        }
    }
 
    // draw cells
    for (int celly=0; celly<cells_in_y_dir; celly++)
    {
        for (int cellx=0; cellx<cells_in_x_dir; cellx++)
        {
            int drawX = cellx * cellSize;
            int drawY = celly * cellSize;
 
            int mx = drawX + cellSize/2;
            int my = drawY + cellSize/2;
 
            rectangle(visu, Point(drawX*zoomFac,drawY*zoomFac), Point((drawX+cellSize)*zoomFac,(drawY+cellSize)*zoomFac), CV_RGB(100,100,100), 1);
 
            // draw in each cell all 9 gradient strengths
            for (int bin=0; bin<gradientBinSize; bin++)
            {
                float currentGradStrength = gradientStrengths[celly][cellx][bin];
 
                // no line to draw?
                if (currentGradStrength==0)
                    continue;
 
                float currRad = bin * radRangeForOneBin + radRangeForOneBin/2;
 
                float dirVecX = cos( currRad );
                float dirVecY = sin( currRad );
                float maxVecLen = cellSize/2;
                float scale = 2.5; // just a visualization scale, to see the lines better
 
                // compute line coordinates
                float x1 = mx - dirVecX * currentGradStrength * maxVecLen * scale;
                float y1 = my - dirVecY * currentGradStrength * maxVecLen * scale;
                float x2 = mx + dirVecX * currentGradStrength * maxVecLen * scale;
                float y2 = my + dirVecY * currentGradStrength * maxVecLen * scale;
 
                // draw gradient visualization
                line(visu, Point(x1*zoomFac,y1*zoomFac), Point(x2*zoomFac,y2*zoomFac), CV_RGB(0,255,0), 1);
 
            } // for (all bins)
 
        } // for (cellx)
    } // for (celly)
 
 
    // don't forget to free memory allocated by helper data structures!
    for (int y=0; y<cells_in_y_dir; y++)
    {
      for (int x=0; x<cells_in_x_dir; x++)
      {
           delete[] gradientStrengths[y][x];            
      }
      delete[] gradientStrengths[y];
      delete[] cellUpdateCounter[y];
    }
    delete[] gradientStrengths;
    delete[] cellUpdateCounter;
 
    return visu;
 
} // get_hogdescriptor_visu

void compute_hog( const vector< Mat > & img_lst, vector< Mat > & gradient_lst, const Size & size )
{
    HOGDescriptor hog;
    hog.winSize = size;
    Mat gray;
    vector< Point > location;
    vector< float > descriptors;

    auto& img = img_lst.begin();
    auto& end = img_lst.end();
    for( ; img != end ; ++img )
    {
        cvtColor( *img, gray, COLOR_BGR2GRAY );
        hog.compute( gray, descriptors, Size( 8, 8 ), Size( 0, 0 ), location );
        gradient_lst.push_back( Mat( descriptors ).clone() );
#ifdef _DEBUG
        imshow( "gradient", get_hogdescriptor_visu( img->clone(), descriptors, size ) );
        waitKey( 10 );
#endif
    }
}

void train_svm( const vector< Mat > & gradient_lst, const vector< int > & labels )
{
    SVM svm;

    /* Default values to train SVM */
    SVMParams params;
    params.coef0 = 0.0;
    params.degree = 3;
    params.term_crit.epsilon = 1e-3;
    params.gamma = 0;
    params.kernel_type = SVM::LINEAR;
    params.nu = 0.5;
    params.p = 0.1; // for EPSILON_SVR, epsilon in loss function?
    params.C = 0.01; // From paper, soft classifier
    params.svm_type = SVM::EPS_SVR; // C_SVC; // EPSILON_SVR; // may be also NU_SVR; // do regression task

    Mat train_data;
    convert_to_ml( gradient_lst, train_data );

    clog << "Start training...";
    svm.train( train_data, Mat( labels ), Mat(), Mat(), params );
    clog << "...[done]" << endl;

    svm.save( "my_people_detector.yml" );
}

void draw_locations( Mat & img, const vector< Rect > & locations, const Scalar & color )
{
    if( !locations.empty() )
    {
        auto& loc = locations.begin();
        auto& end = locations.end();
        for( ; loc != end ; ++loc )
        {
            rectangle( img, *loc, color, 2 );
        }
    }
}

void test_it( const Size & size )
{
    char key = 27;
    Scalar reference( 0, 255, 0 );
    Scalar trained( 0, 0, 255 );
    Mat img, draw;
    SVM svm;
    HOGDescriptor hog;
    HOGDescriptor my_hog;
    my_hog.winSize = size;
    VideoCapture video;
    vector< Rect > locations;

    // Load the trained SVM.
    svm.load( "my_people_detector.yml" );
    // Set the trained svm to my_hog
    vector< float > hog_detector;
    svm.get_svm_detector( hog_detector );
    my_hog.setSVMDetector( hog_detector );
    // Set the people detector.
    hog.setSVMDetector( hog.getDefaultPeopleDetector() );
    // Open the camera.
    video.open(0);
    if( !video.isOpened() )
    {
        cerr << "Unable to open the device 0" << endl;
        exit( -1 );
    }
    
    while( true )
    {
        video >> img;
        if( !img.data )
            break;

        draw = img.clone();
        
        locations.clear();
        hog.detectMultiScale( img, locations );
        draw_locations( draw, locations, reference );

        locations.clear();
        my_hog.detectMultiScale( img, locations );
        draw_locations( draw, locations, trained );

        imshow( "Video", draw );
        key = waitKey( 10 );
        if( 27 == key )
            break;
    }
}

int main( int argc, char** argv )
{
    if( argc != 4 )
    {
        cout << "Wrong number of parameters." << endl
             << "Usage: " << argv[0] << " pos_dir pos.lst neg_dir neg.lst" << endl
             << "example: " << argv[0] << " /INRIA_dataset/ Train/pos.lst /INRIA_dataset/ Train/neg.lst" << endl;
        exit( -1 );
    }
    vector< Mat > pos_lst;
    vector< Mat > full_neg_lst;
    vector< Mat > neg_lst;
    vector< Mat > gradient_lst;
    vector< int > labels;

    load_images( argv[1], argv[2], pos_lst );
    labels.assign( pos_lst.size(), +1 );
    const int old = labels.size();
    load_images( argv[3], argv[4], full_neg_lst );
    sample_neg( full_neg_lst, neg_lst, Size( 96,160 ) );
    labels.insert( labels.end(), neg_lst.size(), -1 );
    CV_Assert( old < labels.size() );

    compute_hog( pos_lst, gradient_lst, Size( 96, 160 ) );
    compute_hog( neg_lst, gradient_lst, Size( 96, 160 ) );

    train_svm( gradient_lst, labels );

    test_it( Size( 96, 160 ) ); // change with your parameters

    return 0;
}
