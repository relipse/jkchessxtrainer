/***************************************************************************
 *   (C) 2005-2006 Marius Roets <roets.marius@gmail.com>                   *
 *   (C) 2005-2009 Michal Rudolf <mrudolf@kdewebdev.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QtDebug>
#include <QFile>
#include "game.h"

QMap<quint64, QString> Game::m_ecoPositions;

Game::Game()
{
    clearTags();
    clear();
    setModified(false);
}

Game::Game(const Game& game)
{
    *this = game;
    setModified(false);
}

Game& Game::operator=(const Game & game)
{
	if (this!=&game)
	{
        clearTags();
		clear();
		//assign non pointer variables
		m_tags = game.m_tags;
		m_startingBoard = game.m_startingBoard;
		m_variationStartAnnotations = game.m_variationStartAnnotations;
        m_annotations = game.m_annotations;
        m_squareAnnotations = game.m_squareAnnotations;
        m_arrowAnnotations = game.m_arrowAnnotations;
        m_clkAnnotations = game.m_clkAnnotations;
        m_egtAnnotations = game.m_egtAnnotations;

		m_currentNode = game.m_currentNode;
		m_startPly = game.m_startPly;
		m_currentBoard = game.m_currentBoard;

        m_moveNodes = game.m_moveNodes;

        setModified(true);
	}
	return *this;
}

Game::~Game()
{
}

MoveId Game::addMove(const Move& move, const QString& annotation, NagSet nags)
{
	MoveNode node;
	MoveId previousNode = m_currentNode;

	node.nextNode = NO_MOVE;
	node.previousNode = m_currentNode;
	node.parentNode = m_moveNodes[m_currentNode].parentNode;
	node.move = move;
	node.nags = nags;
	node.ply = ply() + 1;
	m_moveNodes.append(node);
	m_currentNode = m_moveNodes.size() - 1;
	if (!annotation.isEmpty())
		setAnnotation(annotation);
	m_moveNodes[previousNode].nextNode = m_currentNode;
	m_currentBoard.doMove(move);
    setModified(true);

	return m_currentNode;
}

MoveId Game::addMove(const QString& sanMove, const QString& annotation, NagSet nags)
{
	Move move = m_currentBoard.parseMove(sanMove);
    if (move.isLegal() || move.isNullMove())
		return addMove(move, annotation, nags);
	return NO_MOVE;
}

bool Game::mergeNode(Game& otherGame)
{
    SaveRestoreMove saveThis(*this);
    SaveRestoreMove saveOther(otherGame);

    QString ann;
    NagSet nags;
    otherGame.forward();
    QString san = otherGame.moveToSan(MoveOnly, PreviousMove, CURRENT_MOVE, &ann, &nags);

    bool retVal =  (NO_MOVE != addMove(san,ann,nags));
    return retVal;
}

bool Game::mergeVariations(Game& otherGame)
{
    QList<MoveId> variationList = otherGame.variations();
    bool ok=true;
    if (variationList.size())
    {
        int otherCurrent = otherGame.currentMove();
        for (QList<MoveId>::iterator iter = variationList.begin(); iter != variationList.end(); ++iter)
        {
            otherGame.enterVariation(*iter);
            if (!mergeAsVariation(otherGame))
            {
                ok = false;
            }
            otherGame.moveToId(otherCurrent);
            if (!ok) break;
        }
    }
    return ok;
}

bool Game::mergeAsMainline(Game& otherGame)
{
    SaveRestoreMove saveNode(*this);

    QString ann;
    NagSet nags;

    QString san = otherGame.moveToSan(MoveOnly, PreviousMove, CURRENT_MOVE, &ann, &nags);

    if (NO_MOVE != addMove(san,ann,nags))
    {
        while (!otherGame.atLineEnd())
        {
            forward();
            if (!mergeNode(otherGame))
            {
                return false;
            }
            mergeVariations(otherGame);
            otherGame.forward();
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool Game::mergeAsVariation(Game& otherGame)
{
    SaveRestoreMove saveNode(*this);

    QString ann;
    NagSet nags;

    QString san = otherGame.moveToSan(MoveOnly, PreviousMove, CURRENT_MOVE, &ann, &nags);

    if (NO_MOVE != addVariation(san,ann,nags))
    {
        while (!otherGame.atLineEnd())
        {
            forward();
            if (!mergeNode(otherGame))
            {
                return false;
            }
            mergeVariations(otherGame);
            otherGame.forward();
        }
    }
    else
    {
        return false;
    }
    return true;
}

MoveId Game::findMergePoint(const Game& otherGame)
{
    MoveId prevNode = NO_MOVE;
    MoveId trailNode = NO_MOVE;
    MoveId otherMergeNode = NO_MOVE;
    bool found = false;
    do
    {
        if (NO_MOVE == (otherMergeNode = otherGame.findPosition(board())))
        {
            if (trailNode != NO_MOVE)
            {
                found = true;
                otherMergeNode = trailNode;
                break;
            }
        }
        else
        {
            prevNode = trailNode;
            trailNode = otherMergeNode;
        }
    } while (forward());

    if (!found && atLineEnd())
    {
        // Both games are identical up to the end of this game
        // Need to go one move back in the game to merge
        otherMergeNode = prevNode;
    }

    if (otherMergeNode == NO_MOVE)
    {
        if (otherGame.m_moveNodes.size() > 0)
        {
            otherMergeNode = 0;
        }
    }

    // otherMergeNode points to the move before the game diverges
    // If the complete games needs to be merged, it points to node 0

    return otherMergeNode;
}

void Game::mergeWithGame(const Game& g)
{
    MoveId saveNode = m_currentNode;
    Game otherGame = g;

    otherGame.moveToEnd();

    // Set the game information on the last move so that it is merged into this game
    QString white = otherGame.tag(TagNameWhite);
    QString black = otherGame.tag(TagNameBlack);
    QString event = otherGame.eventInfo();
    QString shortDescription = QString("%1-%2 %3").arg(white).arg(black).arg(event);
    otherGame.setAnnotation(shortDescription);

    MoveId otherMergeNode = findMergePoint(otherGame);

    // todo

    if (otherMergeNode != NO_MOVE)
    {
        backward();
        // merge othergame starting with otherMergeNode into variation starting from m_currentNode
        otherGame.moveToId(otherMergeNode);
        otherGame.forward();
        if (atLineEnd())
        {
            mergeAsMainline(otherGame);
        }
        else
        {
            mergeAsVariation(otherGame);
        }
        otherGame.moveToId(otherMergeNode);
        mergeVariations(otherGame);
    }

    // undo changes
    if (saveNode != m_currentNode)
    {
        moveToId(saveNode);
    }
    compact();
}

QString Game::eventInfo() const
{
    QString result;
    QString site = tag(TagNameSite).left(30).remove("?");
    QString event = tag(TagNameEvent).left(30).remove("?");
    if (!site.isEmpty()) {
        result.append(site);
        if (tag("Round") != "?")
            result.append(QString(" (%1)").arg(tag("Round")));
        if (!event.isEmpty())
            result.append(", ");
    }
    result.append(event);
    if (!tag("Date").startsWith("?")) {
        if (result.length() > 4)
            result.append(", ");
        result.append(tag(TagNameDate).remove(".??"));
    }
    return result;
}

bool Game::currentNodeHasVariation(Square from, Square to) const
{
    if (m_currentNode == NO_MOVE) return false;

    QList<MoveId> vs = m_moveNodes[m_currentNode].variations;
    QList<MoveId>::iterator i;
    for (i = vs.begin(); i != vs.end(); ++i)
    {
        Move m = move(*i);
        if( m.from() == from && m.to() == to )
        {
            return true;
        }
    }
    return false;
}

bool Game::currentNodeHasMove(Square from, Square  to) const
{
    if (currentNodeHasVariation(from,to))
    {
        return true;
    }
    int node = m_moveNodes[m_currentNode].nextNode;
    if (node == NO_MOVE)
        return true;
    Move m = m_moveNodes[node].move;
    if( m.from() == from && m.to() == to )
    {
        return (m_moveNodes[node].nextNode != NO_MOVE);
    }
    return false;
}

// does the next main move or one of the variations go from square from to square to
// if so make it on the board
bool Game::findNextMove(Square from, Square to, PieceType promotionPiece)
{
    int node;
    node = m_moveNodes[m_currentNode].nextNode;
    if( node != NO_MOVE ) {
        Move m = m_moveNodes[node].move ;
        if( m.from() == from && m.to() == to &&
                ((promotionPiece == None) || ((m.isPromotion() && (pieceType( m.promotedPiece()) == promotionPiece)))))
        {
            forward();
            return true;
        }
        else
        {
            QList<MoveId> vs = m_moveNodes[m_currentNode].variations;
            QList<MoveId>::iterator i;
            for (i = vs.begin(); i != vs.end(); ++i)
            {
                Move m = move(*i);
                if( m.from() == from && m.to() == to &&
                        ((promotionPiece == None) || ((m.isPromotion() && (pieceType( m.promotedPiece()) == promotionPiece)))))
                {
                    moveToId(*i);
                    return true;
                }
            }
         }
    }
    return false;
}


bool Game::replaceMove(const Move& move, const QString& annotation, NagSet nags, bool bReplace)
{
	int node;
	node = m_moveNodes[m_currentNode].nextNode;
    setModified(true);
	if (node == NO_MOVE) {
		addMove(move, annotation, nags);
		return true;
	}

	//replace node data with new move
	m_moveNodes[node].move = move;
	m_moveNodes[node].nags = nags;
	setAnnotation(annotation, node);

	//remove any following nodes after replaced move by disconnecting them from the tree
	forward();
    if (bReplace)
    {
        truncateVariation();
    }
    else
    {
        truncateVariationAfterNextIllegalPosition();

    }
	backward();

	return true;
}

bool Game::replaceMove(const QString& sanMove)
{
    return replaceMove(m_currentBoard.parseMove(sanMove), QString(), NagSet(), true);
}

bool Game::insertMove(Move m)
{
    return replaceMove(m, annotation(), nags(), false);
}

MoveId Game::addVariation(const Move& move, const QString& annotation, NagSet nags)
{
    MoveId previousNode = m_currentNode;
	MoveId saveNextNode = m_moveNodes[m_currentNode].nextNode;
	MoveId node = addMove(move, annotation, nags);
	m_moveNodes[m_currentNode].parentNode = previousNode;
	m_moveNodes[previousNode].variations.append(node);
	m_moveNodes[previousNode].nextNode = saveNextNode;

	return (m_moveNodes.size() - 1);
}

MoveId Game::addVariation(const MoveList& moveList, const QString& annotation)
{
	if (moveList.isEmpty())
		return NO_MOVE;
	MoveId currentPosition = currentMove();
	MoveId varStart;
	int start = 1;
    if (!atLineEnd()) {
		varStart = addVariation(moveList.first());
	}
	else if (!atGameStart()) {
		Move oldMove = move();
		backward();
		varStart = addVariation(oldMove);
		start = 0;
	}
	else varStart = addMove(moveList.first());
	moveToId(varStart);
    for (int i = start; i < moveList.count(); ++i) {
		addMove(moveList[i]);
		forward();
	}
	if (!annotation.isEmpty())
		setAnnotation(annotation);
	moveToId(currentPosition);
	return varStart;
}

MoveId Game::addVariation(const QString& sanMove, const QString& annotation, NagSet nags)
{
	MoveId previousNode = m_currentNode;
	MoveId saveNextNode = m_moveNodes[m_currentNode].nextNode;
	MoveId node = addMove(sanMove, annotation, nags);
	if (node == NO_MOVE) return node;

	m_moveNodes[m_currentNode].parentNode = previousNode;
	m_moveNodes[previousNode].variations.append(node);
	m_moveNodes[previousNode].nextNode = saveNextNode;

	return (m_moveNodes.size() - 1);
}

bool Game::promoteVariation(MoveId variation)
{
	if (isMainline(variation))
		return false;

	MoveId currentNode = m_currentNode;	// Save current move

	// Find first move of the variation
	while (!atLineStart(variation))
		variation = m_moveNodes[variation].previousNode;
	MoveId parent = m_moveNodes[variation].parentNode;

	// Link old main line to parent 
	reparentVariation(m_moveNodes[parent].nextNode, parent);
	// Link new main line to parent's parent
	reparentVariation(variation, m_moveNodes[parent].parentNode);

	// Swap main line and the variation
	int index = m_moveNodes[parent].variations.indexOf(variation);
	qSwap(m_moveNodes[parent].nextNode, m_moveNodes[parent].variations[index]);

	moveToId(currentNode);	// Restore current move
	return true;
}

bool Game::removeVariation(MoveId variation)
{
	// don't remove whole game
	if (!variation)
		return false;
	MoveId parentNode = m_moveNodes[variation].parentNode;
	removeNode(variation);
	moveToId(parentNode);

	QList<MoveId> &vars = m_moveNodes[m_currentNode].variations;
	vars.removeAt(vars.indexOf(variation));

	compact();
	return true;
}

void Game::truncateVariationAfterNextIllegalPosition()
{
    SaveRestoreMove saveNode(*this);
    QList<MoveId> variationList = variations();
    for (QList<MoveId>::iterator iter = variationList.begin(); iter != variationList.end(); ++iter)
    {
        SaveRestoreMove saveNodeLoop(*this);
        enterVariation(*iter);
        truncateVariationAfterNextIllegalPosition();
    }
    QString san = moveToSan();
    Game g = *this;
    if (NO_MOVE == g.addMove(san))
    {
        int node = m_moveNodes[m_currentNode].nextNode;
        removeNode(node);
    }
    else
    {
        if (forward())
        {
            truncateVariationAfterNextIllegalPosition();
        }
    }
}

void Game::truncateVariation(Position position)
{
	if (position == AfterMove) {
		int node = m_moveNodes[m_currentNode].nextNode;
		removeNode(node);
	}
    else if (position == BeforeMove && m_currentNode != 0)
    {
        if (atLineStart())
        {
            backward();
            forward();
        }
		MoveId current = m_currentNode;
		MoveNode firstNode;
		firstNode.nextNode = m_currentNode;
		firstNode.ply = m_moveNodes[m_currentNode].ply - 1;
		// Keep variation if truncating main line
		if (m_moveNodes[m_moveNodes[m_currentNode].previousNode].nextNode == m_currentNode) {
			firstNode.variations = m_moveNodes[m_moveNodes[m_currentNode].previousNode].variations;
			foreach(MoveId var, firstNode.variations) {
				reparentVariation(var, 0);
				m_moveNodes[var].previousNode = 0;
			}
		}
		m_moveNodes[0] = firstNode;
		m_moveNodes[m_currentNode].previousNode = 0;
		backward();
		m_startingBoard = m_currentBoard;
        if (m_startingBoard != standardStartBoard)
        {
            m_tags[TagNameFEN] = m_startingBoard.toFen();
            m_tags[TagNameSetUp] = "1";
        }
		moveToId(current);
	}
	compact();
}

const Board& Game::board() const
{
	return m_currentBoard;
}

Board Game::startingBoard() const
{
	return m_startingBoard;
}

QString Game::toFen() const
{
	return m_currentBoard.toFen();
}

QString Game::toHumanFen() const
{
    return m_currentBoard.toHumanFen();
}

QString Game::gameComment() const
{
    return composeAnnotation(m_annotations[0],0);
}

void Game::setGameComment(const QString& gameComment)
{
    setAnnotation(gameComment, 0);
}

void Game::removeVariations()
{
    for (int i = 0; i < m_moveNodes.size(); ++i)
    {
        m_moveNodes[i].variations.clear();
    }
    compact();
    setModified(true);
}

void Game::removeComments()
{
    m_variationStartAnnotations.clear();
    m_annotations.clear();
    m_squareAnnotations.clear();
    m_arrowAnnotations.clear();
    m_clkAnnotations.clear();
    m_egtAnnotations.clear();
    for (int i = 0; i < m_moveNodes.size(); ++i)
    {
        m_moveNodes[i].nags.clear();
    }
    compact();
    setModified(true);
}

bool Game::isMainline(MoveId moveId) const
{
	if (moveId == 0) return true;
	MoveId node = nodeValid(moveId);
	if (node == NO_MOVE)
		return false;
    else
        return m_moveNodes[node].parentNode == NO_MOVE;
}

Result Game::result() const
{
	if (m_tags["Result"] == "1-0") {
		return WhiteWin;
	} else if (m_tags["Result"] == "1/2-1/2") {
		return Draw;
	} else if (m_tags["Result"] == "0-1") {
		return BlackWin;
	} else {
        return ResultUnknown;
	}
}

int Game::numberOfSiblings(MoveId moveId) const
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return 0;
    MoveId parentNode = m_moveNodes[node].parentNode;
    if (parentNode == NO_MOVE)
        return 0;
    return m_moveNodes[parentNode].variations.size();
}

bool Game::atLineStart(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node == NO_MOVE) 
		return false;
    return (m_moveNodes[node].previousNode == m_moveNodes[node].parentNode)
		|| m_moveNodes[node].previousNode == 0;
}

bool Game::atGameStart(MoveId moveId) const
{
	return (nodeValid(moveId) == 0);
}

bool Game::atGameEnd(MoveId moveId) const
{
    return (atLineEnd(moveId) && isMainline(moveId));
}

bool Game::atLineEnd(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		if (m_moveNodes[node].nextNode == NO_MOVE) {
			return true;
		}
	}

	return false;
}

bool Game::setAnnotation(QString annotation, MoveId moveId, Position position)
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return false;

    setModified(true);

    QString specAnnot = specialAnnotation(annotation, "[%csl");
    if( position == Game::AfterMove || node == 0 )
    {
        setSquareAnnotation(specAnnot,node);
    }
    else
    {
        setSquareAnnotation(specAnnot,m_moveNodes[node].previousNode);
    }

    specAnnot = specialAnnotation(annotation, "[%cal");
    if( position == Game::AfterMove || node == 0)
    {
        setArrowAnnotation(specAnnot,node);
    }
    else
    {
        setArrowAnnotation(specAnnot,m_moveNodes[node].previousNode);
    }

    specAnnot = specialAnnotation(annotation, "[%egt");
    if( position == Game::AfterMove || node == 0)
    {
        setEgtAnnotation(specAnnot,node);
    }
    else
    {
        setEgtAnnotation(specAnnot,m_moveNodes[node].previousNode);
    }

    specAnnot = specialAnnotation(annotation, "[%clk");
    if( position == Game::AfterMove || node == 0)
    {
        setClkAnnotation(specAnnot,node);
    }
    else
    {
        setClkAnnotation(specAnnot,m_moveNodes[node].previousNode);
    }

    annotation.remove(QRegExp("\\[%csl[^\\]]*\\]"));
    annotation.remove(QRegExp("\\[%cal[^\\]]*\\]"));
    annotation.remove(QRegExp("\\[%clk[^\\]]*\\]"));
    annotation.remove(QRegExp("\\[%egt[^\\]]*\\]"));
    int moves;
    int comments;
    int nags;
    moveCount(&moves,&comments,&nags);

	if (position == AfterMove) {
		if (annotation.isEmpty())
			m_annotations.remove(node);
		else m_annotations[node] = annotation;
	} 
	else if (canHaveStartAnnotation(node)) {	// Pre-move comment
		if (annotation.isEmpty())
			m_variationStartAnnotations.remove(node);
        else
            m_variationStartAnnotations[node] = annotation;
	}
	return true;
}

bool Game::setSquareAnnotation(QString squareAnnotation, MoveId moveId)
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return false;

    if (squareAnnotation.isEmpty())
        m_squareAnnotations.remove(node);
    else m_squareAnnotations[node] = squareAnnotation;
    return true;
}

bool Game::appendSquareAnnotation(Square s, QChar colorCode)
{
    setModified(true);
    QString newAnnot;
    QString annot = squareAnnotation();
    QString oldAnnot = annot;
    QString sq = strSquareNames[s];
    if (annot.isEmpty())
    {
        if (colorCode != QChar(0))
        {
            newAnnot = QString("%1%2").arg(colorCode).arg(sq);
        }
    }
    else
    {
        annot.replace(QRegExp(QString(",.")+sq),""); 	// if not first annotation remove annotation with preceding comma
        annot.replace(QRegExp(QString(".")+sq+","),""); // if first annotation remove annotation with trailing comma
        annot.replace(QRegExp(QString(".")+sq),""); 	// if only annotation remove annotation
          if (colorCode != QChar(0))
        {
            if( annot.isEmpty())
            {
                newAnnot = QString("%1%2").arg(colorCode).arg(sq);	// If only annotation don't add comma
            } else
            {
                newAnnot = QString("%1,%2%3").arg(annot).arg(colorCode).arg(sq);	// if not only annotation add comma
            }
        }
        else
        {
            if (!annot.isEmpty())
            {
                newAnnot = annot;
            }
        }
        newAnnot.replace(" ,"," ");
        newAnnot.replace(",,",",");
        if (newAnnot.endsWith("'")) newAnnot.truncate(newAnnot.length()-1);
     }
    setSquareAnnotation(newAnnot);
    return true;
}

bool Game::appendArrowAnnotation(Square dest, Square src, QChar colorCode)
{
    if ((src == InvalidSquare) || (dest == InvalidSquare))
            return false;
    setModified(true);
    QString newAnnot;
    QString annot = arrowAnnotation();
    QString sqSrc = strSquareNames[src];
    QString sqDest = strSquareNames[dest];
    if (annot.isEmpty())
    {
        if (colorCode != QChar(0))
        {
            newAnnot = QString("%1%2%3").arg(colorCode).arg(sqSrc).arg(sqDest);
        }
    }
    else
    {

        annot.replace(QRegExp(QString(",.")+sqSrc+sqDest),""); 	// if not first annotation remove annotation with preceding comma
        annot.replace(QRegExp(QString(".")+sqSrc+sqDest+","),""); // if first annotation remove annotation with trailing comma
        annot.replace(QRegExp(QString(".")+sqSrc+sqDest),""); 	// if only annotation remove annotation

        if (colorCode != QChar(0))
        {
            newAnnot = QString("%1,%2%3%4").arg(annot).arg(colorCode).arg(sqSrc).arg(sqDest);
        }
        else
        {
            if (!annot.isEmpty())
            {
                newAnnot = annot;
            }
        }
        newAnnot.replace(" ,"," ");
        newAnnot.replace(",,",",");
        if (newAnnot.endsWith("'")) newAnnot.truncate(newAnnot.length()-1);
    }
    setArrowAnnotation(newAnnot);
    return true;
}

QString Game::squareAnnotation(MoveId moveId) const
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return "";

    QString annotation = m_squareAnnotations[node];
    if(annotation.isNull()) {
        return "";
    }
    return m_squareAnnotations[node].simplified();
}

bool Game::setArrowAnnotation(QString arrowAnnotation, MoveId moveId)
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return false;

    if (arrowAnnotation.isEmpty())
        m_arrowAnnotations.remove(node);
    else
        m_arrowAnnotations[node] = arrowAnnotation;
    return true;
}

QString Game::arrowAnnotation(MoveId moveId) const
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return "";

    QString annotation = m_arrowAnnotations[node];
    if(annotation.isNull()) {
        return "";
    }
    return m_arrowAnnotations[node].simplified();
}

bool Game::setEgtAnnotation(QString annotation, MoveId moveId)
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return false;

    if (annotation.isEmpty())
        m_egtAnnotations.remove(node);
    else
        m_egtAnnotations[node] = annotation;
    return true;
}

QString Game::egtAnnotation(MoveId moveId) const
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return "";

    QString annotation = m_egtAnnotations[node];
    if(annotation.isNull()) {
        return "";
    }
    return m_egtAnnotations[node];
}

QString Game::timeAnnotation(MoveId moveId) const
{
    QString s = clkAnnotation(moveId);
    if (s.isEmpty())
    {
        return egtAnnotation(moveId);
    }
    return s;
}

bool Game::setClkAnnotation(QString annotation, MoveId moveId)
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return false;

    if (annotation.isEmpty())
        m_clkAnnotations.remove(node);
    else
        m_clkAnnotations[node] = annotation;
    return true;
}

QString Game::clkAnnotation(MoveId moveId) const
{
    MoveId node = nodeValid(moveId);
    if (node == NO_MOVE)
        return "";

    QString annotation = m_clkAnnotations[node];
    if(annotation.isNull()) {
        return "";
    }
    return m_clkAnnotations[node];
}

QString Game::composeAnnotation(QString freeText, MoveId node) const
{
    if (!m_squareAnnotations[node].isEmpty())
    {
        freeText.append(QString("[%csl %1]").arg(m_squareAnnotations[node]));
    }
    if (!m_arrowAnnotations[node].isEmpty())
    {
        freeText.append(QString("[%cal %1]").arg(m_arrowAnnotations[node]));
    }
    if (!m_clkAnnotations[node].isEmpty())
    {
        freeText.append(QString("[%clk %1]").arg(m_clkAnnotations[node]));
    }
    if (!m_egtAnnotations[node].isEmpty())
    {
        freeText.append(QString("[%egt %1]").arg(m_egtAnnotations[node]));
    }
    return freeText;
}

QString Game::annotation(MoveId moveId, Position position) const
{
	MoveId node = nodeValid(moveId);
	if (position == AfterMove)
        return composeAnnotation(m_annotations.value(node, ""),node);
    else
    {
        if (node == 0)
            return gameComment();
        else
            return m_variationStartAnnotations.value(node, "");
    }
}

bool Game::canHaveStartAnnotation(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	return atLineStart(moveId) || atGameStart(m_moveNodes[node].previousNode);
}

bool Game::addNag(Nag nag, MoveId moveId)
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		m_moveNodes[node].nags.addNag(nag);
        setModified(true);
		return true;
	}
	return false;
}

bool Game::setNags(NagSet nags, MoveId moveId)
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		m_moveNodes[node].nags = nags;
        setModified(true);
		return true;
	}
	return false;
}

bool Game::clearNags(MoveId moveId)
{
    return setNags(NagSet(),moveId);
}

NagSet Game::nags(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		return m_moveNodes[node].nags;
	}
	return NagSet();
}

MoveId Game::nodeValid(MoveId moveId) const
{
	if (moveId == CURRENT_MOVE) {
		moveId = m_currentNode;
	}
	if ((moveId >= 0) && (moveId < m_moveNodes.size())) {
		if (m_moveNodes[moveId].removed) {
			return NO_MOVE;
		}
		return moveId;
	}
	return NO_MOVE;
}

void Game::moveCount(int* moves, int* comments, int* nags) const
{
	*moves = *comments = *nags = 0;

	int node = 1;
	while (nodeValid(node) != NO_MOVE) {
		*moves += 1;
		if (m_moveNodes[node].nags.count() != 0) {
			*nags += 1;
		}
		node = m_moveNodes[node].nextNode;
	}
	// Count comments
	for (int i = 0; i < m_annotations.size(); ++i) {
		*comments += 1;
	}
	for (int i = 0; i < m_variationStartAnnotations.size(); ++i) {
		*comments += 1;
	}
}

int Game::ply(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		return m_moveNodes[node].ply;
	}
	return 0;
}

int Game::moveNumber(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		if (int plyNum = ply(node))
			return (m_startPly + plyNum - 1) / 2 + 1;
		return 0;
	}
	return -1;
}

int Game::plyCount() const
{
	int count = 0;
	int node = 0;

	while (node != NO_MOVE) {
        ++count;
		node = m_moveNodes[node].nextNode;
	}
	// Counted one too much, because we have to start at zero
	// (an empty game has no node 1)
	return count - 1;
}

bool Game::canMoveVariationUp(MoveId moveId) const
{
    if (isMainline()) return false;

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moveNodes[moveId].parentNode;

    const QList <MoveId>& v = m_moveNodes[parentNode].variations;
    int i = v.indexOf(variation);
    return (i > 0);
}

void Game::moveVariationUp(MoveId moveId)
{
    if (isMainline()) return;

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moveNodes[moveId].parentNode;

    QList <MoveId>& v = m_moveNodes[parentNode].variations;
    int i = v.indexOf(variation);
    if (i > 0)
    {
        v.swap(i, i-1);
        setModified(true);
    }
}

bool Game::canMoveVariationDown(MoveId moveId) const
{
    if (isMainline()) return false;

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moveNodes[moveId].parentNode;

    const QList <MoveId>& v = m_moveNodes[parentNode].variations;
    int i = v.indexOf(variation);
    return (i>=0 && (i+1) < v.count());
}

void Game::moveVariationDown(MoveId moveId)
{
    if (isMainline()) return;

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moveNodes[moveId].parentNode;

    QList <MoveId>& v = m_moveNodes[parentNode].variations;
    int i = v.indexOf(variation);
    if (i>=0 && (i+1) < v.count())
    {
        v.swap(i, i+1);
        setModified(true);
    }
}

void Game::enumerateVariations(MoveId moveId, char a)
{
    if (isMainline()) return;

    MoveId node = nodeValid(moveId);
    if (node != NO_MOVE)
    {
        MoveId parentNode = m_moveNodes[node].parentNode;
        QList <MoveId>& v = m_moveNodes[parentNode].variations;
        for (int i = 0; i < v.size(); ++i)
        {
            QString oldAnnotation = annotation(v[i], Game::BeforeMove);
            oldAnnotation.remove(QRegExp("^.\\)"));
            QString s=QString("%1) %2").arg(QChar(a+i)).arg(oldAnnotation).trimmed();
            setAnnotation(s, v[i], Game::BeforeMove);
        }
    }
}

MoveId Game::variationNumber(MoveId moveId) const
{
	if (isMainline()) return 0;
    MoveId node = nodeValid(moveId);
    if (node != NO_MOVE)
    {
        MoveId parentNode = m_moveNodes[node].parentNode;

        while (m_moveNodes[node].previousNode != parentNode)
        {
            node = m_moveNodes[node].previousNode;
        }
    }
    return node;
}

MoveId Game::mainLineMove() const
{
    MoveId node = nodeValid(m_currentNode);
    if (node != NO_MOVE)
    {
        bool dive = false;
        while (m_moveNodes[node].parentNode != NO_MOVE)
        {
            dive = true;
            node = m_moveNodes[node].parentNode;
        }
        if (dive) node = m_moveNodes[node].nextNode;
    }
    return node;
}

MoveId Game::previousMove() const
{
	return m_moveNodes[m_currentNode].previousNode;
}

MoveId Game::nextMove() const
{
	return m_moveNodes[m_currentNode].nextNode;
}

MoveId Game::parentMove() const
{
	return m_moveNodes[m_currentNode].parentNode;
}

MoveId Game::currentMove() const
{
	return m_currentNode;
}

const QList<MoveId>& Game::variations() const
{
	return m_moveNodes[m_currentNode].variations;
}

int Game::variationCount(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		return m_moveNodes[node].variations.size();
	}
	return 0;
}

bool Game::variationHasSiblings(MoveId variation) const
{
    if (isMainline(variation))
        return false;
    while (!atLineStart(variation))
        variation = m_moveNodes[variation].previousNode;
    MoveId parent = m_moveNodes[m_currentNode].parentNode;
    return (variationCount(parent) > 1);
}

bool Game::isModified() const
{
	return m_isModified;
}

void Game::setModified(bool set)
{
	m_isModified = set;
}

void Game::indicateAnnotationsOnBoard(MoveId moveId)
{
    QString annotation = squareAnnotation(moveId);
    m_currentBoard.setSquareAnnotation(annotation);

    annotation = arrowAnnotation(moveId);
    m_currentBoard.setArrowAnnotation(annotation);
}

void Game::moveToStart()
{
	m_currentNode = 0;
	m_currentBoard = m_startingBoard;

    indicateAnnotationsOnBoard(m_currentNode);
}

int Game::moveByPly(int diff)
{
	if (diff > 0) {
		return forward(diff);
	} else {
		return backward(-diff);
	}
}

void Game::moveToId(MoveId moveId)
{
	if (nodeValid(moveId) == NO_MOVE)
		return;

	//jump to node, travelling back to start adding the moves to the stack
	int node = moveId;
	QStack < Move > moveStack;
	while (node) {
		moveStack.push(m_moveNodes[node].move);
		node = m_moveNodes[node].previousNode;
	}

	//reset the board, then make the moves on the stack to create the correct position
	m_currentNode = moveId;
	m_currentBoard = m_startingBoard;
	while (!moveStack.isEmpty()) {
		m_currentBoard.doMove(moveStack.pop());
	}

    indicateAnnotationsOnBoard(moveId);
}

int Game::moveToPly(int ply)
{
	if (ply == 0) {
		moveToStart();
		return 0;
	}

	int currentPly = this->ply();
	int diff = ply - currentPly;

	if (diff > 0) {
		return currentPly + forward(diff);
	} else {
		return currentPly - backward(-diff);
	}
}

Move Game::move(MoveId moveId) const
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
		return m_moveNodes[node].move;
	}
	return Move();
}

void Game::moveToEnd()
{
	// Move out of variations to mainline
	while (m_moveNodes[m_currentNode].parentNode != NO_MOVE) {
		moveToId(m_moveNodes[m_currentNode].parentNode);
	}
	// Now move forward to the end of the game
	while (m_moveNodes[m_currentNode].nextNode != NO_MOVE) {
		forward(1);
    }
}

int Game::forward(int count)
{
	int moved = 0;
	while ((m_moveNodes[m_currentNode].nextNode != NO_MOVE) && (moved < count)) {
		m_currentNode = m_moveNodes[m_currentNode].nextNode;
        ++moved;

		m_currentBoard.doMove(m_moveNodes[m_currentNode].move);
	}

    if (moved)
        indicateAnnotationsOnBoard(m_currentNode);

	return moved;
}

int Game::backward(int count)
{
	int moved = 0;
	while ((m_moveNodes[m_currentNode].previousNode >= 0) && (moved < count)) {
		m_currentBoard.undoMove(m_moveNodes[m_currentNode].move);
		m_currentNode = m_moveNodes[m_currentNode].previousNode;
        ++moved;
	}

    if (moved)
        indicateAnnotationsOnBoard(m_currentNode);

	return moved;
}

void Game::enterVariation(const MoveId& moveId)
{
	Q_ASSERT(variations().contains(moveId));
	m_currentBoard.doMove(m_moveNodes[moveId].move);
	m_currentNode = moveId;

    indicateAnnotationsOnBoard(m_currentNode);
}

void Game::removeNode(MoveId moveId)
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE) {
        setModified(true);

        if (variationCount(node)) {
			for (int i = 0; i < m_moveNodes[node].variations.size(); ++i) {
				removeNode(m_moveNodes[node].variations[i]);
			}
		}
		removeNode(m_moveNodes[node].nextNode);
		if (!atLineStart(node)) {
			m_moveNodes[m_moveNodes[node].previousNode].nextNode = NO_MOVE;
		}
		m_moveNodes[node].remove();
	}
}

void Game::clear()
{
	m_moveNodes.clear();
	m_variationStartAnnotations.clear();
	m_annotations.clear();
    m_squareAnnotations.clear();
    m_arrowAnnotations.clear();
    m_clkAnnotations.clear();
    m_egtAnnotations.clear();

	m_startPly = 0;
	m_currentNode = 0;

	m_startingBoard.setStandardPosition();
	m_currentBoard = m_startingBoard;

	m_moveNodes.append(MoveNode());
    setModified(true);
}

void Game::clearTags()
{
	m_tags.clear();
    setModified(true);
}

QString Game::tag(const QString& tag) const
{
	return m_tags[tag];
}

const QMap<QString, QString>& Game::tags() const
{
	return m_tags;
}

void Game::setTag(const QString& tag, const QString& value)
{
	m_tags[tag] = value;
}

void Game::removeTag(const QString& tag)
{
	m_tags.remove(tag);
}

void Game::setStartingBoard(const Board& startingBoard)
{
    setStartingBoard(startingBoard.toFen());
}

void Game::setStartingBoard(const QString& fen)
{
    clear();
	m_startingBoard.fromFen(fen);
    if (m_startingBoard != standardStartBoard)
    {
        m_tags[TagNameFEN] = fen;
        m_tags[TagNameSetUp] = "1";
    }
	m_currentBoard = m_startingBoard;
	m_startPly = (m_startingBoard.moveNumber() - 1) * 2 + (m_startingBoard.toMove() == Black);
}

void Game::setResult(Result result)
{
	m_tags["Result"] = resultString(result);
    setModified(true);
}

QString Game::moveToSan(MoveStringFlags flags, NextPreviousMove nextPrevious, MoveId moveId,
                        QString* annots, NagSet* nagSet)
{
	MoveId node = nodeValid(moveId);
	if (node != NO_MOVE && nextPrevious == NextMove)
		node = m_moveNodes[node].nextNode;
	if (node == NO_MOVE)
		return QString();

	MoveNode move;
	move = m_moveNodes[node];
    if (!( move.move.isLegal() || move.move.isNullMove()))
		return QString();

	// Save current node
	MoveId saveNode = NO_MOVE;
	MoveId boardNode = m_moveNodes[node].previousNode;
	if (boardNode != m_currentNode) {
		saveNode = m_currentNode;
		moveToId(boardNode);
	}

	// Move number
	QString san;
	if (m_currentBoard.toMove() == Black && flags & BlackNumbers) 
		san += QString::number(moveNumber(node)) + "... ";
	else if (m_currentBoard.toMove() == White && flags & WhiteNumbers) 
		san += QString::number(moveNumber(node)) + ". ";

	// Move and SAN
    san += m_currentBoard.moveToSan(move.move, flags & TranslatePiece);
	if (flags & Nags)
    {
        san += nags(node).toString(NagSet::Simple);
    }

    if (nagSet) *nagSet = nags(node);
    if (annots) *annots = annotation(node);

	// Restore previous position
	if (saveNode != NO_MOVE)
		moveToId(saveNode);
	
	return san;
}

void Game::dumpMoveNode(MoveId moveId)
{
	if (moveId == CURRENT_MOVE) {
		moveId = m_currentNode;
	}
	if (moveId != NO_MOVE) {
		qDebug() << "Move Id : " << moveId;
		qDebug() << "   Next node   : " << m_moveNodes[moveId].nextNode;
		qDebug() << "   Prev node   : " << m_moveNodes[moveId].previousNode;
		qDebug() << "   Parent node : " << m_moveNodes[moveId].parentNode;
        qDebug() << "   Nags        : " << m_moveNodes[moveId].nags.toString(NagSet::Simple);
		qDebug() << "   Deleted     : " << m_moveNodes[moveId].removed;
		qDebug() << "   # Variations: " << m_moveNodes[moveId].variations.size();
		qDebug() << "   Variations  : " << m_moveNodes[moveId].variations;
		qDebug() << "   Move        : " << moveToSan(FullDetail, PreviousMove, moveId);
	}
}

void Game::dumpAnnotations(MoveId moveId)
{
    if (moveId == CURRENT_MOVE) {
        moveId = m_currentNode;
    }
    if (moveId != NO_MOVE) {
        qDebug() << "   Annotations : ";
        qDebug() << "   Text        : " << m_annotations[moveId];
        qDebug() << "   Square      : " << m_squareAnnotations[moveId];
        qDebug() << "   Arrow       : " << m_arrowAnnotations[moveId];
        qDebug() << "   CLK         : " << m_clkAnnotations[moveId];
        qDebug() << "   EGT         : " << m_egtAnnotations[moveId];
        qDebug() << "   Start       : " << m_variationStartAnnotations[moveId];
    }
}


void Game::dumpAllMoveNodes()
{
    qDebug() << endl;
	qDebug() << "Current Node: " << m_currentNode;
	for (int i = 0; i < m_moveNodes.size(); ++i) {
		dumpMoveNode(i);
        dumpAnnotations(i);
	}
	int moves, comments, nags;
	moveCount(&moves, &comments, &nags);
    qDebug() << "Moves: " << moves << " Comments: " << comments << " Nags: " << nags;
}

int Game::findPosition(const Board& position) const
{
    MoveId current = 0;
    Board currentBoard(m_startingBoard);

    for (;;) {
        if (currentBoard == position && currentBoard.positionIsSame(position))
            return current;

        current = m_moveNodes[current].nextNode;
        if (current == NO_MOVE || !position.canBeReachedFrom(currentBoard))
            return NO_MOVE;

        currentBoard.doMove(m_moveNodes[current].move);
    }
    return NO_MOVE;
}

bool Game::loadEcoFile(const QString& ecoFile)
{
	QFile file(ecoFile);
    if (file.open(QIODevice::ReadOnly))
    {
        QDataStream sin(&file);
        quint32 id;
        sin >> id;
        if (id == COMPILED_ECO_FILE_ID)
        {
            sin >> m_ecoPositions;
            return true;
        }
        return false;
    }
    return false;
}

void Game::compact()
{
	int oldSize = m_moveNodes.size();
	QList <MoveNode> moveNodes;
	QVector <int> oldIdNewIdMapping(oldSize, NO_MOVE);

	for (int i = 0; i < oldSize; ++i) {
		if (!m_moveNodes[i].removed) {
			oldIdNewIdMapping[i] = moveNodes.size();
			moveNodes.append(m_moveNodes[i]);
		}
	}

	// update nodes links to other nodes in shrinked list (prev, next, variations)
	for (int i = 0, newSize = moveNodes.size(); i < newSize; ++i) {
		MoveNode& node = moveNodes[i];
#define GAME_UPDATE_MOVEID(aMoveId) if (aMoveId != NO_MOVE) aMoveId = oldIdNewIdMapping[aMoveId]
		GAME_UPDATE_MOVEID(node.nextNode);
		GAME_UPDATE_MOVEID(node.previousNode);
		GAME_UPDATE_MOVEID(node.parentNode);
		QList<MoveId>& vars = node.variations;
		for (int j = 0; j < vars.size(); ++j)
			GAME_UPDATE_MOVEID(vars[j]);
		vars.removeAll(NO_MOVE);
#undef  GAME_UPDATE_LINK
	}

	m_moveNodes.clear();
	m_moveNodes = moveNodes;
	m_currentNode = oldIdNewIdMapping[m_currentNode];
}

QString Game::ecoClassify()
{
	//move to end of main line
	MoveId currentNode = m_currentNode;
	moveToEnd();

	//search backwards for the first eco position
	while (backward()) {
		quint64 key = m_currentBoard.getHashValue();
		if (m_ecoPositions.contains(key)) {
			moveToId(currentNode);
			return m_ecoPositions[key];
		}
	}
	moveToId(currentNode);

	return QString();
}

bool Game::isEcoPosition() const
{
    quint64 key = m_currentBoard.getHashValue();
    return (m_ecoPositions.contains(key));
}

void Game::reparentVariation(MoveId variation, MoveId parent)
{
    if (variation != NO_MOVE)
        setModified(true);
	for (MoveId node = variation; node != NO_MOVE; node = m_moveNodes[node].nextNode)
		m_moveNodes[node].parentNode = parent;
}

QString Game::specialAnnotation(QString& annotation, QString specialMark ) const // [%csl  [%cal
{
    QString result = "";
    int specialAnnotationStart = annotation.indexOf(specialMark);
    // If we found a chessbase special annotation
    if(specialAnnotationStart >= 0 ) {

        int specialAnnotationDataStart = specialAnnotationStart + specialMark.length() + 1; //+ 1 for space after specialMark
        int specialAnnotationEnd = annotation.indexOf(']', specialAnnotationDataStart);

        if(specialAnnotationEnd > specialAnnotationDataStart )
        {
            result = annotation.mid(specialAnnotationDataStart,specialAnnotationEnd - specialAnnotationDataStart );
            annotation.remove(QRegExp(QString("\\") + specialMark+"[\\]]*\\]"));
        };
    }
    return result;
}


