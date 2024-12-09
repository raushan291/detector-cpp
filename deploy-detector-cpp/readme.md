### Steps to Create a Debian Package for This Project

1. Set Up the Directory Structure
    Create a directory structure for your Debian package. Use the following layout:

    deploy-debian-pkg/  
    ├── DEBIAN/  
    │   └── control  
    ├── usr/  
    │   ├── local/  
    │   │   ├── bin/  
    │   │   │   └── start-myapp  
    │   │   ├── lib/  
    │   │   │   └── (all shared libraries here)  
    │   │   └── exe/  
    │   │       └── Infer_OpenCV_detector_multithreading_multiple_cameras.exe  
    │   └── share/  
    │       ├── myapp/  
    │       │   ├── app.py  
    │       │   ├── models/  
    │       │   │   └── yolov11n-face.onnx  
    │       │   ├── templates/  
    │       │   │   └── index.html  
    │       ├── applications/  
    │       │   └── myapp.desktop  
    │       └── icons/  
    │           └── hicolor/  
    │               └── 48x48/  
    │                   └── apps/  
    │                       └── myapp-icon.png  



2. Create the Control File
    In the DEBIAN/ directory, create a file named control with the following content:

        Package: myapp
        Version: 1.0.0
        Section: utils
        Priority: optional
        Architecture: amd64
        Maintainer: Your Name <your.email@example.com>
        Description: A Flask-based application for multi-camera inference using OpenCV.
         This package provides a precompiled C++ executable for multithreaded inference,
         shared libraries for OpenCV, and a Flask interface to manage streams.


3. Add the Start Script
    In usr/local/bin/, create a script named start-myapp to simplify running the Flask app. This script should look like:

        #!/bin/bash
        export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH
        cd /usr/share/myapp
        python3 app.py

    Make it executable:
        $ chmod +x usr/local/bin/start-myapp

4. (Optional): Create a Desktop Entry
    To make the app accessible via the desktop environment, add a .desktop file in usr/share/applications/:
        [Desktop Entry]
        Name=MyApp
        Comment=Flask-based multi-camera inference
        Exec=start-myapp
        Icon=myapp-icon
        Terminal=true
        Type=Application
        Categories=Utility;

5. (Optional): Add a Custom Icon
    If you want a custom icon:

    Place the icon file (e.g., myapp-icon.png) in the directory deploy-debian-pkg/usr/share/icons/hicolor/48x48/apps/

6. Copy Files
    C++ Executable: Place Infer_OpenCV_detector_multithreading_multiple_cameras.exe into usr/local/exe/.
    Shared Libraries: Place all .so files into usr/local/lib/.
    Flask App: Place app.py, models/, and templates/ into usr/share/myapp/.

7. Build the Debian Package
    $ dpkg-deb --build deploy-debian-pkg

8. Install and Test
    Install the package:
        $ sudo dpkg -i deploy-debian-pkg.deb
    Test it:
        $ start-myapp

If any dependency issues occur during installation, resolve them using:
    $ sudo apt-get install -f
