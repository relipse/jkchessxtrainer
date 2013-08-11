#include "popheronextorplay.h"
#include "ui_popheronextorplay.h"

popHeroNextOrPlay::popHeroNextOrPlay(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::popHeroNextOrPlay)
{
    ui->setupUi(this);
}

popHeroNextOrPlay::~popHeroNextOrPlay()
{
    delete ui;
}
