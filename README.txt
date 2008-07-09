=== WHAT IS THIS ===
This is an application that scans QuakeWorld (KT* enabled) servers and reports final scores from matches. It uses a "smart" technique to scan the servers as little times as possible, yet scan it in a way that no match is missed.
The application reports the results to IRC server and to web server too.

=== LAUNCH ===
Windows: run scoreacc.exe
Linux: run ./scoreacc

=== CONFIGURATION ===
Edit scoreacc.conf file, it should be pretty straight-forward. Documentation may come later.
If not sure what commands you can use, type "help" in the app.
WARNING: package comes with scoreacc.conf with windows-like line-endings
Unless you solve this for yourself, on Linux you need to replace it with unix variant:
rm scoreacc.conf
mv scoreacc.conf.unix scoreacc.conf

=== INSTALLATION ===
There is no installation script, ./scoreacc launches the app.

=== COMPILING ===
To compile on windows, simply open scoreacc.sln and select Build -> Build Solution

To compile on linux, simply type make in src/ dir
The package comes with pre-compiled libircclient library, if you want to compile it for yourself, follow these steps:
1) switch to src/libircclient
2) chmod 744 configure
3) ./configure
4) make

After this, switch to src/ and do make.

=== AUTHOR ===
made by JohnNy_cz
contact: JohnNy_cz @ #qw.cz @ irc://irc.quakenet.org/ 
e-mail: jan.raszyk@gmail.com

=== LICENSE ===
GNU/GPL
http://www.gnu.org/licenses/gpl-2.0.html
