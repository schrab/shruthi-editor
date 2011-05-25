// Shruthi-Editor: An unofficial Editor for the Shruthi hardware synthesizer. For 
// informations about the Shruthi, see <http://www.mutable-instruments.net/shruthi1>. 
//
// Copyright (C) 2011 Manuel Krönig
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QApplication>
#include <QtCore>
#include "lib_editor.h"
#include "lib_config.h"
#include "lib_midiin.h"
#include "shruthi-editor.h"
#include <QDebug>
#include "RtMidi.h"
#include "keyboard-dialog.h"

#ifdef CLEANLOOKS
#include <QCleanlooksStyle>
#endif
// ******************************************
class SignalRouter : public QObject {
// ******************************************
    Q_OBJECT

    private:
        Config config;
    
    public:
        SignalRouter() {
#ifdef DEBUG
            qDebug() << "SignalRouter::SignalRouter()";
#endif
            // load config
            config = Config();
            config.load();
        }
        ~SignalRouter() {
#ifdef DEBUG
            qDebug() << "SignalRouter::~SignalRouter()";
#endif
            editorEnabled=false;
        }
        
        bool editorWorking;
        bool editorEnabled;
        QQueue<queueitem_t> queue;

    
    public slots:
        void run() {
            emit setMidiDevices(config.getMidiInputDevice(),config.getMidiOutputDevice());
            editorEnabled=true;
            editorWorking=false;
        }
        void enqueue(queueitem_t item) {
            if (editorWorking)
                queue.enqueue(item);
            else if (editorEnabled) {
                editorWorking=true;
                
                emit editorProcess(item);
            }
        }
        void editorFinished() {
            if (editorEnabled) {
                if (!queue.isEmpty())
                    emit editorProcess(queue.dequeue());
                else
                    editorWorking=false;
            }
        }
        
        
        void midiDeviceChanged(int in, int out) {
#ifdef DEBUG
            qDebug() << "midiDeviceChanged: in" << in << ", out:" <<out;
#endif
            emit setMidiDevices(in,out);
            bool config_changed = (config.getMidiInputDevice() != in) || (config.getMidiOutputDevice() != out);
            if (config_changed) {
                config.setMidiInputDevice(in);
                config.setMidiOutputDevice(out);
#ifdef DEBUG
                qDebug() << "Config was changed. Saving.";
#endif
                config.save();
            }
        }

    signals:
        void editorProcess(queueitem_t);
        void setMidiDevices(int,int);
};

// ******************************************
int main(int argc, char *argv[]) {
// ******************************************
    qRegisterMetaType<queueitem_t>("queueitem_t");
    
#ifdef CLEANLOOKS    
    QApplication::setStyle(new QCleanlooksStyle);
#endif    
    
    int retVal;
    
    {
        // Setup signalrouter
        SignalRouter sr;
        QThread srThread;
        sr.connect(&srThread, SIGNAL(started()),SLOT(run()));
        sr.moveToThread(&srThread);
        
        // Setup editor
        Editor editor;
        QThread editorThread;
        editor.moveToThread(&editorThread);
        // editor: incoming signals
        editor.connect(&sr, SIGNAL(editorProcess(queueitem_t)),SLOT(process(queueitem_t)));
        editor.connect(&sr, SIGNAL(setMidiDevices(int,int)),SLOT(setMidiDevices(int,int)));

        // Setup midiin
        MidiIn midiin;
        QThread midiinThread;
        midiin.moveToThread(&midiinThread);
        midiinThread.start();
        // midiin: incoming signals
        midiin.connect(&sr, SIGNAL(setMidiDevices(int,int)),SLOT(setMidiDevices(int,int)));

        // Setup main_window
        QApplication app(argc, argv);
        shruthiEditorMainWindow *main_window = new shruthiEditorMainWindow(&editor);
        main_window->setWindowIcon(QIcon("shruthi-editor.png"));
        main_window->setFixedSize(main_window->width(),main_window->height());
        main_window->statusBar()-> setSizeGripEnabled ( false );
        main_window->setAttribute(Qt::WA_DeleteOnClose, true);
        // main_window: incoming signals
        main_window->connect(&editor,SIGNAL(redrawNRPN(int)),SLOT(redrawNRPN(int)));
        main_window->connect(&editor,SIGNAL(redrawAll()),SLOT(redrawAll()));
        main_window->connect(&sr,SIGNAL(setMidiDevices(int,int)),SLOT(setMidiDevices(int,int)));
        main_window->connect(&editor,SIGNAL(midiOutputStatusChanged(bool)), SLOT(midiOutputStatusChanged(bool)));
        main_window->connect(&editor,SIGNAL(displayStatusbar(QString)), SLOT(displayStatusbar(QString)));
        main_window->connect(&midiin,SIGNAL(midiInputStatusChanged(bool)),SLOT(midiInputStatusChanged(bool)));

        // Setup keyboard
        keyboard keys;
        keys.connect(main_window,SIGNAL(showKeyboard()),SLOT(showKeyboard()));
        keys.setWindowIcon(QIcon("shruthi-editor.png"));
        keys.setFixedSize(keys.width(),keys.height());
        
        // Start editor
        editorThread.start();

        // signalrouter: incoming signals
        sr.connect(&editor, SIGNAL(finished()),SLOT(editorFinished()));
        sr.connect(main_window,SIGNAL(enqueue(queueitem_t)),SLOT(enqueue(queueitem_t)));
        sr.connect(&midiin,SIGNAL(enqueue(queueitem_t)),SLOT(enqueue(queueitem_t)));
        sr.connect(&keys,SIGNAL(enqueue(queueitem_t)),SLOT(enqueue(queueitem_t)));
        sr.connect(main_window,SIGNAL(midiDeviceChanged(int,int)),SLOT(midiDeviceChanged(int,int)));

        // start signal router
        srThread.start();
        
        main_window->show();
        retVal= app.exec();
#ifdef DEBUG
        qDebug() << "main: shutting down...";
        
        qDebug() << "srThread.quit()";
#endif
        srThread.quit();
        srThread.wait(1000);

#ifdef DEBUG
        qDebug() << "editorThread.quit()";
#endif
        editorThread.quit();
        editorThread.wait(1000);
#ifdef DEBUG
        qDebug() << "midiinThread.quit()";
#endif
        midiinThread.quit();
        midiinThread.wait(1000);
    }
    
#ifdef DEBUG
    qDebug() << "return"<< retVal;
#endif
    return retVal;
}

#include "main.moc"
