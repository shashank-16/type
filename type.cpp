//include section

#include<ctype.h>
#include<cstdio>
#include<errno.h>
#include<iostream>
#include<termios.h>
#include<stdlib.h>
#include<sys/ioctl.h>
#include<unistd.h>


#define CTRL_KEY(k) ((k) & 0x1f)

//data section 
struct editorConfig{

    int screenrows;
    int screencols;

struct termios orig_termios;

};

struct editorConfig E;

//terminal section 

void die(const char* s)
{
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);
    perror(s);
    exit(1);

}

void disableRawMode()
{
    if( tcsetattr(STDIN_FILENO,TCSAFLUSH, &E.orig_termios) ==-1)
    {
        die("tcsetattr");

    }

}



void enableRawMode(){

    if (tcgetattr(STDIN_FILENO,&E.orig_termios)== -1)
    {
        die("tcgetattr");

    }
    atexit(disableRawMode);


    struct termios raw= E.orig_termios;
    raw.c_iflag &=~(IXON | ICRNL|BRKINT|INPCK);
    raw.c_oflag &=~(OPOST);
    raw.c_cflag |= (CS8);


    raw.c_lflag &= ~(ECHO | ICANON| IEXTEN | ISIG);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=1;

   if ( tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==-1 ) die("tcsetattr");


}

char editorReadkey()
{
    int nread;
    char a;
    while((nread = read(STDIN_FILENO,&a,1 ))!=1)
    {
        if(nread == -1 && errno !=EAGAIN ) die ("read");

    }
    return a;

}

void editorProcessKeypress()
{
    char c= editorReadkey();

    switch(c)
    {
        case CTRL_KEY('q'):
        write(STDOUT_FILENO,"\x1b[2J",4);
        write(STDOUT_FILENO,"\x1b[H",3);
        exit(0);
        break;

    }
}

void editorDrawRows()
{
    int y;
    for(y=0;y<E.screenrows;y++)
    {
        write(STDOUT_FILENO,"~~",2); // increase these to add symbol at first line of rows 

        if(y<E.screenrows -1 )
        {
            write(STDOUT_FILENO,"\r\n",2);

        }

    }
}

void editorrefreshScreen()
{
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);
    editorDrawRows();
    write(STDOUT_FILENO,"\x1b[H",3);




}

int getCursorPosition(int* rows ,int* cols)
{
    char buf[32];
    unsigned int i=0;

    if(write(STDOUT_FILENO,"\x1b[6n",4)!=4) return -1;

  while(i<sizeof(buf)-1)
  {
      if(read(STDIN_FILENO,&buf[i],1)!=1)break;
      if(buf[i]=='R') break;
      i++;

  }
buf[i]='\0';

if(buf[0] != '\x1b' || buf[1]!= '[') return -1;
if(sscanf(&buf[2], "%d;%d",rows,cols) !=2 )return -1;


    //   printf("\r\n&buf[1]: '%s'\r\n",&buf[1]);

    // editorReadkey();
    return 0;

}

int getWindowSize(int* rows,int* cols )
{
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)== -1 || ws.ws_col==0)
    {
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12 ) return -1;
        // editorReadkey(); //  i think something will happen here <---------------------------------------------------------------
        getCursorPosition(rows,cols);
        return -1;

    }else{
        *cols=ws.ws_col;
        *rows=ws.ws_row;
        return 0;

    }
}


void initEditor()
{
    if(getWindowSize(&E.screenrows,&E.screencols)== -1 ) die("getWindowSize");
}
//init 

int main()
{
   
    // std::cout<<"        ......././././.. Thanks for Downlaoding type Text editor ...././././././";

    enableRawMode();
    initEditor();


    
    while(1){
        editorrefreshScreen();

      editorProcessKeypress();

    }

    return 0;
    
}