#include "Processor.h"

//-------------
// Constructor
//-------------
Processor::Processor(QObject *parent) : QObject(parent)
{
    isBusy = true; // Initially no image has been loaded so triggers are ignored

    // Connect the internal trigger used to make generation calculation asynchronous
    connect(this, SIGNAL(internalGenerationTrigger()), this, SLOT(performGenerationCalculation()));
}

//-----------------------------------------------------------------------------------
// Perform preprocessing (grayscale conversion and thresholding to black and white)
// on given image. Given image is copied to internal member for further processing
//-----------------------------------------------------------------------------------
const QImage &Processor::PreprocessImage(const QImage &image)
{
    // Set busy flag to prevent triggering during preprocessing
    isBusy = true;

    // Make local copy of the original image for further processing
    localImageCopy = image.copy();

    // Loop through all pixels in the image
    for (int x = 0; x < image.width(); x++)
    {
        for (int y = 0; y < image.height(); y++)
        {
            // Get color value of pixel and extract the red, green and blue color components from it
            // for the grayscale conversion and thresholding.
            // NOTE: simple averaging might not be the best way of performing the grayscaling since
            // the red, green and blue color components do not have the same apparent brightness
            // (specifically the blue color component appears less bright than the green and red
            // color components). Therefore a weighted averaging of color components might be a
            // better method. This weighted averaging would be implemented using averaging formula:
            // (Wr*red + Wg*green + Wb*blue)/(Wr+Wg+Wb)
            // Since color components are integers this weighting would require either converting
            // them to floats (if the weighting factors were decimal values) or using integer
            // weighting values such as 120, 120 and 90 for Wr, Wg and Wb, respectively
            QColor rgb = image.pixelColor(x, y);
            int red = rgb.red();
            int green = rgb.green();
            int blue = rgb.blue();
            int average = (red + green + blue)/3;

            // Setting pixel color to grayscale (used in testing the grayscale conversion)
            //localImageCopy.setPixelColor(x, y, QColor(average, average, average));

            // Thresholding sets pixel color to black (RGB value #000000) if average is < 128
            // and to white (RGB value #FFFFFF) if average is >= 128
            if (average < 128)
                localImageCopy.setPixelColor(x, y, QColor(0, 0, 0));
            else
                localImageCopy.setPixelColor(x, y, QColor(255, 255, 255));
        }
    }

    // Now that image has been processed processor is ready for generation calculations
    isBusy = false;

    // Return reference to processed copy of original image
    return localImageCopy;
}

//-------------------------------------------------------------------------------------------
// Slot function used by external trigger (e.g. timer) to attempt to trigger calculation of
// new generation on current image. Trigger is ignored if processor is busy
//-------------------------------------------------------------------------------------------
void Processor::triggerGenerationCalculation()
{
    // Nothing will be done if the processor is currently busy (or has no image available)
    if (isBusy)
        return;

    // Internal generation trigger signal is used to make calculation triggering asynchronous.
    // If the generation function was just called from here the call would be stuck until the
    // calculation was done
    isBusy = true; // Mark processor as busy to prevent re-entrant triggers
    emit internalGenerationTrigger();
}

//----------------------------------------------------------
// Slot function used to actually calculate next generation
//----------------------------------------------------------
void Processor::performGenerationCalculation()
{
    // Make temporary copy of original image. This copy is used for making the generation
    // calculations while the internal image m_image is modified. Same image can't be used
    // for both generation calculation and drawing because changing the image would
    // result in further generation calculations becoming corrupted
    QImage copyImage = localImageCopy.copy();

    // Loop through all pixels in the copy of the original image
    for (int x = 0; x < copyImage.width(); x++)
    {
        for (int y = 0; y < copyImage.height(); y++)
        {
            // Get state of this cell to determine if it retains it state, dies or comes alive.
            // If the cell retains its current state nothing is done to this pixel
            int thisCellState = cellState(copyImage, x, y);

            // Count number of living neighbours. This might be a little faster if instead of
            // using two nested loops each of the 8 neighbours was checked separately, but
            // the difference would probably be negligible
            int livingNeighbours = 0;
            for (int dx = -1; dx <= 1; dx++)
            {
                for (int dy = -1; dy <= 1; dy++)
                {
                    // Ignore this cell in calculation (dx=dy=0)
                    if ((dx != 0) || (dy != 0))
                    {
                        livingNeighbours += cellState(copyImage, x+dx, y+dy);
                    }
                }
            }

            // Apply rules to cell depending on its current state
            if (thisCellState == 0)
            {
                // Cell is dead. See if number of living neighbours causes it to come alive.
                // Exactly 3 living neighbours are needed for dead cell to come alive.
                if (livingNeighbours == 3)
                {
                    // Cell comes alive => set pixel to white
                    localImageCopy.setPixelColor(x, y, QColor(255, 255, 255));
                }
            }
            else
            {
                // Cell is alive. See if number of living neighbours causes it to die.
                // Living cell dies if it has less than two living neighbours (isolation) or
                // more than three living neighbours (overpopulation)
                if ((livingNeighbours < 2) || (livingNeighbours > 3))
                {
                    // Cell dies due to isolation or overpopulation => set pixel to black
                    localImageCopy.setPixelColor(x, y, QColor(0, 0, 0));
                }
            }
        }
    }

    // Processing done. Send modified imago to view class
    emit postNewGenerationImage(localImageCopy);

    // Now that new generation has been calculated processor is once again ready for next round
    isBusy = false;
}

//-------------------------------------------------------------------------------------------------------
// Check if specified cell (determined by coordinates) is alive (white) or dead (black).
// Returns value 0 if dead and value 1 if alive. This way it is easy to calculate number of living
// neighbours of any cell.
// Cell is automatically considered to be dead if it is outside image boundaries
//-------------------------------------------------------------------------------------------------------
int Processor::cellState(const QImage &image, int x, int y)
{
    // Check out of bounds case first
    if ((x < 0) || (y < 0) || (x >= image.width()) || y >= image.height())
        return 0; // Out of bounds => dead

    // Image is assumed to be black and white so checking one color component is enough
    QColor rgb = image.pixelColor(x, y);
    int red = rgb.red();
    if (red < 128)
        return 0; // Black => dead

    return 1;
}
