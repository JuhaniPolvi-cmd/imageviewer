/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtWidgets>
#ifndef QT_NO_PRINTER
#include <QPrintDialog>
#endif

#include "imageviewer.h"

//-----------------------------------
// Constructor
//-----------------------------------
ImageViewer::ImageViewer()
   : imageLabel(new QLabel)
   , scrollArea(new QScrollArea)
   , processor(new Processor)
   , timer(new QTimer)
   , scaleFactor(1)
{
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setVisible(false);
    setCentralWidget(scrollArea);

    createActions();

    resize(QGuiApplication::primaryScreen()->availableSize() * 3 / 5);

    // Connect new image post signal from processor to the image update slot function
    connect(processor, SIGNAL(postNewGenerationImage(const QImage &)), this, SLOT(updateImage(const QImage &)));

    // Set up the timer used to calculate new generations
    timer->setInterval(1000); // 1 second interval
    timer->setSingleShot(false); // Continuous timer

    // Connect timer timeout signal to processor trigger slot
    connect(timer, SIGNAL(timeout()), processor, SLOT(triggerGenerationCalculation()));

    // Start the timer. Since the processor is initially set to be busy until an image has been set
    // it will ignore timeouts until an image has been processed and set
    timer->start();
}

//------------------------------------------------
// Load specified image file and set it to viewer
//------------------------------------------------
bool ImageViewer::loadFile(const QString &fileName)
{
    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage newImage = reader.read();
    if (newImage.isNull()) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Cannot load %1: %2")
                                 .arg(QDir::toNativeSeparators(fileName), reader.errorString()));
        return false;
    }

    // Preprocess the image using the processor before setting it.
    // Preprocessing the image will also make a local copy of it to the processor.
    setImage(processor->PreprocessImage(newImage));

    setWindowFilePath(fileName);

    const QString message = tr("Opened \"%1\", %2x%3, Depth: %4")
        .arg(QDir::toNativeSeparators(fileName)).arg(image.width()).arg(image.height()).arg(image.depth());
    statusBar()->showMessage(message);
    return true;
}

//-------------------------------------------------------------------------------------------------
// Slot function used to trigger updating the set image. Triggered by update signal from processor
//-------------------------------------------------------------------------------------------------
void ImageViewer::updateImage(const QImage &image)
{
    // Use modified version of setImage, since original setImage resets scaling factor to 1.
    // This would undo any zoom in/zoom out committed by the user.
    setImageWithoutChangingScalingFactor(image);
}

//-----------------------------------------------------
// Set specified image to the image view
//-----------------------------------------------------
void ImageViewer::setImage(const QImage &newImage)
{
    image = newImage;
    imageLabel->setPixmap(QPixmap::fromImage(image));
    scaleFactor = 1.0;

    scrollArea->setVisible(true);
    printAct->setEnabled(true);
    fitToWindowAct->setEnabled(true);
    updateActions();

    if (!fitToWindowAct->isChecked())
        imageLabel->adjustSize();
}

//--------------------------------------------------------------------------------------------
// Set image to window while retaining current scaling factor. This function is a copy of
// setImage with the exception of not setting the scaling factor. Function was added because
// using setImage to update the image after the processor had calculated new generation
// would reset any changes to scaling factor. The original setImage is still used when
// opening a new image file
//--------------------------------------------------------------------------------------------
void ImageViewer::setImageWithoutChangingScalingFactor(const QImage &newImage)
{
    image = newImage;
    imageLabel->setPixmap(QPixmap::fromImage(image));
    imageLabel->resize(scaleFactor * imageLabel->pixmap()->size());

    scrollArea->setVisible(true);
    printAct->setEnabled(true);
    fitToWindowAct->setEnabled(true);
    updateActions();

    if (!fitToWindowAct->isChecked())
        imageLabel->adjustSize();

    // Invoke scaleImage with scaling factor of 1 to perform necessary scaling settings
    // (this action will not change the scaling factor, but without it the new image posted
    // by the processor will reset the image to default size)
    scaleImage(1);
}

//-----------------------------------------------------------------------------------------
// Save the image currently in the view. This function will perform the actual writing of
// image data to specified image file and is called by saveAs
//-----------------------------------------------------------------------------------------
bool ImageViewer::saveFile(const QString &fileName)
{
    QImageWriter writer(fileName);

    if (!writer.write(image)) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Cannot write %1: %2")
                                 .arg(QDir::toNativeSeparators(fileName)), writer.errorString());
        return false;
    }
    const QString message = tr("Wrote \"%1\"").arg(QDir::toNativeSeparators(fileName));
    statusBar()->showMessage(message);
    return true;
}

//--------------------------------------------------------------------------------------------
// Initialize the file dialog. This file dialog is used for both opening an image file to be
// used as a base for the generation calculation by the processor and also for saving the
// current image in the view
//--------------------------------------------------------------------------------------------
static void initializeImageFileDialog(QFileDialog &dialog, QFileDialog::AcceptMode acceptMode)
{
    static bool firstDialog = true;

    if (firstDialog) {
        firstDialog = false;
        const QStringList picturesLocations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        dialog.setDirectory(picturesLocations.isEmpty() ? QDir::currentPath() : picturesLocations.last());
    }

    QStringList mimeTypeFilters;
    const QByteArrayList supportedMimeTypes = acceptMode == QFileDialog::AcceptOpen
        ? QImageReader::supportedMimeTypes() : QImageWriter::supportedMimeTypes();
    foreach (const QByteArray &mimeTypeName, supportedMimeTypes)
        mimeTypeFilters.append(mimeTypeName);
    mimeTypeFilters.sort();
    dialog.setMimeTypeFilters(mimeTypeFilters);
    dialog.selectMimeTypeFilter("image/jpeg");
    if (acceptMode == QFileDialog::AcceptSave)
        dialog.setDefaultSuffix("jpg");
}

//-----------------------------------------------------------------------------
// Open image file to be used as a base for the generation calculation.
// This image will be first converted to grayscale and then to black and white
// using thresholding
//-----------------------------------------------------------------------------
void ImageViewer::open()
{
    // Stop the timer before opening the dialog to prevent updating the image during open
    timer->stop();

    QFileDialog dialog(this, tr("Open File"));
    initializeImageFileDialog(dialog, QFileDialog::AcceptOpen);

    while (dialog.exec() == QDialog::Accepted && !loadFile(dialog.selectedFiles().first())) {}

    // Restart the timer after the dialog is done
    timer->start();
}

//----------------------------------------------------------
// Save the currently viewed image to file under given name
//----------------------------------------------------------
void ImageViewer::saveAs()
{
    // Stop the timer for the duration of save function to prevent image update during saving
    timer->stop();

    QFileDialog dialog(this, tr("Save File As"));
    initializeImageFileDialog(dialog, QFileDialog::AcceptSave);

    while (dialog.exec() == QDialog::Accepted && !saveFile(dialog.selectedFiles().first())) {}

    // Restart the timer after finishing the dialog
    timer->start();
}

//----------------------------------
// Print the currently viewed image
//----------------------------------
void ImageViewer::print()
{
    Q_ASSERT(imageLabel->pixmap());
#if !defined(QT_NO_PRINTER) && !defined(QT_NO_PRINTDIALOG)
    QPrintDialog dialog(&printer, this);
    if (dialog.exec()) {
        QPainter painter(&printer);
        QRect rect = painter.viewport();
        QSize size = imageLabel->pixmap()->size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(imageLabel->pixmap()->rect());
        painter.drawPixmap(0, 0, *imageLabel->pixmap());
    }
#endif
}

//--------------------------------------------------------------------------------------
// Copy the currently viewed image to the clipboard. This can be used to revert back to
// earlier generation
//--------------------------------------------------------------------------------------
void ImageViewer::copy()
{
#ifndef QT_NO_CLIPBOARD
    QGuiApplication::clipboard()->setImage(image);
#endif // !QT_NO_CLIPBOARD
}

#ifndef QT_NO_CLIPBOARD
//------------------------------------------------------------------------------------
// Return the image stored to clipboard. Used in replacing the currently viewed image
// with the image in clipboard
//------------------------------------------------------------------------------------
static QImage clipboardImage()
{
    if (const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData()) {
        if (mimeData->hasImage()) {
            const QImage image = qvariant_cast<QImage>(mimeData->imageData());
            if (!image.isNull())
                return image;
        }
    }
    return QImage();
}
#endif // !QT_NO_CLIPBOARD

//-----------------------------------------------------------------------------------------
// Replace current image with the image in clipboard. The image in clipboard could be from
// either selecting the copy menu selection (which will copy current image on view to
// clipboard) or getting a screen grab
//-----------------------------------------------------------------------------------------
void ImageViewer::paste()
{
#ifndef QT_NO_CLIPBOARD
    const QImage newImage = clipboardImage();
    if (newImage.isNull()) {
        statusBar()->showMessage(tr("No image in clipboard"));
    } else {
        // NOTE: pasting a new image to the view also requires setting the new image to processor.
        // If this is not done the processor will still have the old local image, which will
        // override the new image on next update. Since the clipboard might have a completely new
        // image (e.g. from taking a screen grab) the pasted image is passed through the
        // preprocessor function. If the image on clipboard is copied from the local view it
        // should already be in black and white so the grayscaling and thresholding will have
        // no effect
        // Note 2: using original setImage here will cause any zooming to be reset. If this is not
        // desirable the modified version should be used instead
        setImage(processor->PreprocessImage(newImage));
        setWindowFilePath(QString());
        const QString message = tr("Obtained image from clipboard, %1x%2, Depth: %3")
            .arg(newImage.width()).arg(newImage.height()).arg(newImage.depth());
        statusBar()->showMessage(message);
    }
#endif // !QT_NO_CLIPBOARD
}

//----------------------------------------------------------------------------------------
// Zoom in on the image by multiplying the current scaling factor by value greater than 1
//----------------------------------------------------------------------------------------
void ImageViewer::zoomIn()
{
    scaleImage(1.25);
}

//--------------------------------------------------------------------------------------
// Zoom out on the image by multiplying the current scaling factor by value less than 1
//--------------------------------------------------------------------------------------
void ImageViewer::zoomOut()
{
    scaleImage(0.8);
}

//-----------------------------------------------------------------------
// Restore the image to normal size by resetting the scaling factor to 1
//-----------------------------------------------------------------------
void ImageViewer::normalSize()
{
    imageLabel->adjustSize();
    scaleFactor = 1.0;
}

//----------------------------------------------------------------------------------------------
// Fit the image to the window (image will fill the window even if it is resized) or switch off
// image fitting depending on current state of the relevant setting in the menu
//----------------------------------------------------------------------------------------------
void ImageViewer::fitToWindow()
{
    bool fitToWindow = fitToWindowAct->isChecked();
    scrollArea->setWidgetResizable(fitToWindow);
    if (!fitToWindow)
        normalSize();
    updateActions();
}

//----------------------------------------
// Show information about the application
//----------------------------------------
void ImageViewer::about()
{
    QMessageBox::about(this, tr("About Image Viewer"),
            tr("<p>The <b>Image Viewer</b> example shows how to combine QLabel "
               "and QScrollArea to display an image. QLabel is typically used "
               "for displaying a text, but it can also display an image. "
               "QScrollArea provides a scrolling view around another widget. "
               "If the child widget exceeds the size of the frame, QScrollArea "
               "automatically provides scroll bars. </p><p>The example "
               "demonstrates how QLabel's ability to scale its contents "
               "(QLabel::scaledContents), and QScrollArea's ability to "
               "automatically resize its contents "
               "(QScrollArea::widgetResizable), can be used to implement "
               "zooming and scaling features. </p><p>In addition the example "
               "shows how to use QPainter to print an image.</p>"));
}

//-----------------------------------------------------------------------------------
// Initialise menus for the application and assign them to relevant action functions
//-----------------------------------------------------------------------------------
void ImageViewer::createActions()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *openAct = fileMenu->addAction(tr("&Open..."), this, &ImageViewer::open);
    openAct->setShortcut(QKeySequence::Open);

    saveAsAct = fileMenu->addAction(tr("&Save As..."), this, &ImageViewer::saveAs);
    saveAsAct->setEnabled(false);

    printAct = fileMenu->addAction(tr("&Print..."), this, &ImageViewer::print);
    printAct->setShortcut(QKeySequence::Print);
    printAct->setEnabled(false);

    fileMenu->addSeparator();

    QAction *exitAct = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcut(tr("Ctrl+Q"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    copyAct = editMenu->addAction(tr("&Copy"), this, &ImageViewer::copy);
    copyAct->setShortcut(QKeySequence::Copy);
    copyAct->setEnabled(false);

    QAction *pasteAct = editMenu->addAction(tr("&Paste"), this, &ImageViewer::paste);
    pasteAct->setShortcut(QKeySequence::Paste);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    zoomInAct = viewMenu->addAction(tr("Zoom &In (25%)"), this, &ImageViewer::zoomIn);
    zoomInAct->setShortcut(QKeySequence::ZoomIn);
    zoomInAct->setEnabled(false);

    zoomOutAct = viewMenu->addAction(tr("Zoom &Out (25%)"), this, &ImageViewer::zoomOut);
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    zoomOutAct->setEnabled(false);

    normalSizeAct = viewMenu->addAction(tr("&Normal Size"), this, &ImageViewer::normalSize);
    normalSizeAct->setShortcut(tr("Ctrl+S"));
    normalSizeAct->setEnabled(false);

    viewMenu->addSeparator();

    fitToWindowAct = viewMenu->addAction(tr("&Fit to Window"), this, &ImageViewer::fitToWindow);
    fitToWindowAct->setEnabled(false);
    fitToWindowAct->setCheckable(true);
    fitToWindowAct->setShortcut(tr("Ctrl+F"));

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    helpMenu->addAction(tr("&About"), this, &ImageViewer::about);
    helpMenu->addAction(tr("About &Qt"), &QApplication::aboutQt);
}

//-----------------------------------------------------------------------
// Enable/disable certain menu selections based on current program state
//-----------------------------------------------------------------------
void ImageViewer::updateActions()
{
    saveAsAct->setEnabled(!image.isNull());
    copyAct->setEnabled(!image.isNull());
    zoomInAct->setEnabled(!fitToWindowAct->isChecked());
    zoomOutAct->setEnabled(!fitToWindowAct->isChecked());
    normalSizeAct->setEnabled(!fitToWindowAct->isChecked());
}

//---------------------------------------------------------------------------
// Scale the image by given scaling factor (< 1 -> zoom out, > 1 -> zoom in)
//---------------------------------------------------------------------------
void ImageViewer::scaleImage(double factor)
{
    Q_ASSERT(imageLabel->pixmap());
    scaleFactor *= factor;
    imageLabel->resize(scaleFactor * imageLabel->pixmap()->size());

    // Update scroll bars based on amount of zooming in/out
    adjustScrollBar(scrollArea->horizontalScrollBar(), factor);
    adjustScrollBar(scrollArea->verticalScrollBar(), factor);

    // Scale is limited to range 0.333-3. Zoom in/out menu item is disabled if relevant action
    // would take the scaling factor out of range
    zoomInAct->setEnabled(scaleFactor < 3.0);
    zoomOutAct->setEnabled(scaleFactor > 0.333);
}

//---------------------------------------------------
// Update a scroll bar based on given scaling factor
//---------------------------------------------------
void ImageViewer::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value()
                            + ((factor - 1) * scrollBar->pageStep()/2)));
}
