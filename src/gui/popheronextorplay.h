#ifndef POPHERONEXTORPLAY_H
#define POPHERONEXTORPLAY_H

#include <QDialog>

namespace Ui {
class popHeroNextOrPlay;
}

class popHeroNextOrPlay : public QDialog
{
    Q_OBJECT
    
public:
    explicit popHeroNextOrPlay(QWidget *parent = 0);
    ~popHeroNextOrPlay();
    
public:
    Ui::popHeroNextOrPlay *ui;
};

#endif // POPHERONEXTORPLAY_H
