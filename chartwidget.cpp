/*
 * 文件名: chartwidget.cpp
 * 文件作用: 通用图表组件实现文件
 * 功能描述:
 * 1. 封装 QCustomPlot 基础功能。
 * 2. 实现了 setChartMode，支持单图与叠加图模式切换。
 * 3. 在 Mode_Stacked 下实现了无缝布局、双图例显示及高度自由调整功能。
 * 4. [优化] addEventLine 函数现在会根据模式自动创建贯穿上下坐标系的线条。
 */

#include "chartwidget.h"
#include "ui_chartwidget.h"
#include "chartsetting1.h"
#include "chartsetting2.h"
#include "modelparameter.h"
#include "styleselectordialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QInputDialog>
#include <cmath>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QColorDialog>
#include <QSpinBox>
#include <QComboBox>

// ============================================================================
// 构造与析构
// ============================================================================

ChartWidget::ChartWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartWidget),
    m_dataModel(nullptr),
    m_titleElement(nullptr),
    m_chartMode(Mode_Single),
    m_topRect(nullptr),
    m_bottomRect(nullptr),
    m_legendBottom(nullptr),
    m_marginGroup(nullptr),
    m_interMode(Mode_None),
    m_activeLine(nullptr),
    m_activeText(nullptr),
    m_activeArrow(nullptr),
    m_movingGraph(nullptr),
    m_resizeStartY(0),
    m_startTopRatio(0.5)
{
    ui->setupUi(this);
    m_plot = ui->chart;

    this->setFocusPolicy(Qt::StrongFocus);
    m_plot->setFocusPolicy(Qt::StrongFocus);

    initUi();
    initConnections();
}

ChartWidget::~ChartWidget()
{
    if (m_marginGroup) delete m_marginGroup;
    // m_legendBottom 若在布局中会由 QCP 自动管理内存，否则需手动删除
    delete ui;
}

// ============================================================================
// 初始化与配置
// ============================================================================

void ChartWidget::initUi()
{
    if (m_plot->plotLayout()->rowCount() == 0) m_plot->plotLayout()->insertRow(0);

    // 标题元素初始化
    if (m_plot->plotLayout()->elementCount() > 0 && qobject_cast<QCPTextElement*>(m_plot->plotLayout()->element(0, 0))) {
        m_titleElement = qobject_cast<QCPTextElement*>(m_plot->plotLayout()->element(0, 0));
    } else {
        if(m_plot->plotLayout()->element(0,0) != nullptr) {
            m_plot->plotLayout()->insertRow(0);
        }
        m_titleElement = new QCPTextElement(m_plot, "", QFont("Microsoft YaHei", 12, QFont::Bold));
        m_plot->plotLayout()->addElement(0, 0, m_titleElement);
    }

    setupAxisRect(m_plot->axisRect());

    // 默认图例设置
    m_plot->legend->setVisible(true);
    QFont legendFont("Microsoft YaHei", 9);
    m_plot->legend->setFont(legendFont);
    m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));
    m_plot->legend->setBorderPen(Qt::NoPen);

    if (m_plot->axisRect()) {
        m_plot->axisRect()->insetLayout()->addElement(m_plot->legend, Qt::AlignTop | Qt::AlignRight);
    }

    // 特征线菜单
    m_lineMenu = new QMenu(this);
    QAction* actSlope1 = m_lineMenu->addAction("斜率 k = 1 (井筒储集)");
    connect(actSlope1, &QAction::triggered, this, [=](){ addCharacteristicLine(1.0); });

    QAction* actSlopeHalf = m_lineMenu->addAction("斜率 k = 1/2 (线性流)");
    connect(actSlopeHalf, &QAction::triggered, this, [=](){ addCharacteristicLine(0.5); });

    QAction* actSlopeQuarter = m_lineMenu->addAction("斜率 k = 1/4 (双线性流)");
    connect(actSlopeQuarter, &QAction::triggered, this, [=](){ addCharacteristicLine(0.25); });

    QAction* actHorizontal = m_lineMenu->addAction("水平线 (径向流)");
    connect(actHorizontal, &QAction::triggered, this, [=](){ addCharacteristicLine(0.0); });

    setZoomDragMode(Qt::Horizontal | Qt::Vertical);
}

void ChartWidget::setupAxisRect(QCPAxisRect *rect)
{
    if (!rect) return;
    // 启用顶部和右侧坐标轴（作为边框），但不显示刻度值
    QCPAxis *topAxis = rect->axis(QCPAxis::atTop);
    topAxis->setVisible(true);
    topAxis->setTickLabels(false);
    connect(rect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), topAxis, SLOT(setRange(QCPRange)));

    QCPAxis *rightAxis = rect->axis(QCPAxis::atRight);
    rightAxis->setVisible(true);
    rightAxis->setTickLabels(false);
    connect(rect->axis(QCPAxis::atLeft), SIGNAL(rangeChanged(QCPRange)), rightAxis, SLOT(setRange(QCPRange)));
}

void ChartWidget::initConnections()
{
    connect(m_plot, &MouseZoom::saveImageRequested, this, &ChartWidget::on_btnSavePic_clicked);
    connect(m_plot, &MouseZoom::exportDataRequested, this, &ChartWidget::on_btnExportData_clicked);
    connect(m_plot, &MouseZoom::drawLineRequested, this, &ChartWidget::addCharacteristicLine);
    connect(m_plot, &MouseZoom::settingsRequested, this, &ChartWidget::on_btnSetting_clicked);
    connect(m_plot, &MouseZoom::resetViewRequested, this, &ChartWidget::on_btnReset_clicked);

    connect(m_plot, &MouseZoom::addAnnotationRequested, this, &ChartWidget::onAddAnnotationRequested);
    connect(m_plot, &MouseZoom::lineStyleRequested, this, &ChartWidget::onLineStyleRequested);

    connect(m_plot, &MouseZoom::deleteSelectedRequested, this, &ChartWidget::onDeleteSelectedRequested);
    connect(m_plot, &MouseZoom::editItemRequested, this, &ChartWidget::onEditItemRequested);

    connect(m_plot, &QCustomPlot::mousePress, this, &ChartWidget::onPlotMousePress);
    connect(m_plot, &QCustomPlot::mouseMove, this, &ChartWidget::onPlotMouseMove);
    connect(m_plot, &QCustomPlot::mouseRelease, this, &ChartWidget::onPlotMouseRelease);
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &ChartWidget::onPlotMouseDoubleClick);
}

// ============================================================================
// 图表模式管理 (核心逻辑)
// ============================================================================

void ChartWidget::setChartMode(ChartMode mode) {
    if (m_chartMode == mode) return;
    m_chartMode = mode;

    exitMoveDataMode();

    // 1. 清理工作
    if (m_plot->legend && m_plot->legend->layout()) {
        m_plot->legend->layout()->take(m_plot->legend);
    }
    if (m_legendBottom) {
        if (m_legendBottom->layout()) m_legendBottom->layout()->take(m_legendBottom);
        delete m_legendBottom;
        m_legendBottom = nullptr;
    }
    if (m_marginGroup) {
        delete m_marginGroup;
        m_marginGroup = nullptr;
    }

    int rowCount = m_plot->plotLayout()->rowCount();
    for(int i = rowCount - 1; i > 0; --i) {
        m_plot->plotLayout()->removeAt(i);
    }
    m_plot->plotLayout()->simplify();

    // 2. 重新构建布局
    QCPAxisRect* legendHostRect = nullptr;

    if (mode == Mode_Single) {
        QCPAxisRect* defaultRect = new QCPAxisRect(m_plot);
        m_plot->plotLayout()->addElement(1, 0, defaultRect);
        setupAxisRect(defaultRect);
        m_topRect = nullptr;
        m_bottomRect = nullptr;
        legendHostRect = defaultRect;

        m_plot->plotLayout()->setRowSpacing(5);

    } else if (mode == Mode_Stacked) {
        m_topRect = new QCPAxisRect(m_plot);
        m_bottomRect = new QCPAxisRect(m_plot);

        m_marginGroup = new QCPMarginGroup(m_plot);
        m_topRect->setMarginGroup(QCP::msLeft | QCP::msRight, m_marginGroup);
        m_bottomRect->setMarginGroup(QCP::msLeft | QCP::msRight, m_marginGroup);

        m_plot->plotLayout()->setRowSpacing(0);

        m_plot->plotLayout()->addElement(1, 0, m_topRect);
        m_plot->plotLayout()->addElement(2, 0, m_bottomRect);

        m_plot->plotLayout()->setRowStretchFactor(1, 1);
        m_plot->plotLayout()->setRowStretchFactor(2, 1);

        setupAxisRect(m_topRect);
        setupAxisRect(m_bottomRect);

        // 坐标轴视觉优化
        m_topRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msTop);
        m_topRect->setMargins(QMargins(0,0,0,0));
        QCPAxis* topRectBotAxis = m_topRect->axis(QCPAxis::atBottom);
        topRectBotAxis->setTickLabels(false);
        topRectBotAxis->setVisible(true);

        m_bottomRect->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
        m_bottomRect->setMargins(QMargins(0,0,0,0));
        QCPAxis* botRectTopAxis = m_bottomRect->axis(QCPAxis::atTop);
        botRectTopAxis->setVisible(false);

        // 创建底部专用图例
        m_legendBottom = new QCPLegend();
        m_legendBottom->setVisible(true);
        QFont legendFont("Microsoft YaHei", 9);
        m_legendBottom->setFont(legendFont);
        m_legendBottom->setBrush(QBrush(QColor(255, 255, 255, 200)));
        m_legendBottom->setBorderPen(Qt::NoPen);

        m_topRect->insetLayout()->addElement(m_plot->legend, Qt::AlignTop | Qt::AlignRight);
        m_bottomRect->insetLayout()->addElement(m_legendBottom, Qt::AlignTop | Qt::AlignRight);

        m_plot->setAutoAddPlottableToLegend(false);
        legendHostRect = nullptr;

        connect(m_topRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)),
                m_bottomRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
        connect(m_bottomRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)),
                m_topRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    }

    if (legendHostRect && m_plot->legend) {
        if (legendHostRect->insetLayout()) {
            legendHostRect->insetLayout()->addElement(m_plot->legend, Qt::AlignTop | Qt::AlignRight);
        }
        m_plot->legend->setVisible(true);
        m_plot->setAutoAddPlottableToLegend(true);
    }

    setZoomDragMode(Qt::Horizontal | Qt::Vertical);
    m_plot->replot();
}

ChartWidget::ChartMode ChartWidget::getChartMode() const {
    return m_chartMode;
}

bool ChartWidget::checkHoverSplitLine(const QPoint& pos) {
    if (m_chartMode != Mode_Stacked || !m_topRect || !m_bottomRect) return false;
    int splitY = m_topRect->bottom();
    return std::abs(pos.y() - splitY) < 5;
}

void ChartWidget::clearEventLines() {
    for (auto line : m_eventLines) {
        if (m_plot->hasItem(line)) {
            m_plot->removeItem(line);
        }
    }
    m_eventLines.clear();
}

// [优化] addEventLine: 支持双坐标系贯穿绘制
void ChartWidget::addEventLine(double x, int type) {
    QColor color = (type == 0) ? Qt::red : Qt::green;
    QPen pen(color);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(2);

    // 辅助 Lambda：创建单段线
    auto createLine = [&](QCPAxisRect* rect) -> QCPItemLine* {
        if (!rect) return nullptr;
        QCPItemLine* line = new QCPItemLine(m_plot);

        // 1. 绑定数据坐标轴 (用于 X 轴绝对定位)
        line->start->setAxes(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
        line->end->setAxes(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));

        // 2. [关键修复] 显式绑定 AxisRect (用于 Y 轴 Ratio 相对定位)
        // 这一步确保了 ptAxisRectRatio 能正确识别是相对于 m_bottomRect 还是 m_topRect
        line->start->setAxisRect(rect);
        line->end->setAxisRect(rect);

        line->setClipAxisRect(rect);
        line->setClipToAxisRect(true);

        // 设置坐标类型：X为绝对坐标，Y为比例坐标(0-1)
        line->start->setTypeX(QCPItemPosition::ptPlotCoords);
        line->start->setTypeY(QCPItemPosition::ptAxisRectRatio);
        line->end->setTypeX(QCPItemPosition::ptPlotCoords);
        line->end->setTypeY(QCPItemPosition::ptAxisRectRatio);

        // 绘制从底(1)到顶(0)
        // 注意：在 AxisRectRatio 模式下，0是顶部，1是底部
        line->start->setCoords(x, 1);
        line->end->setCoords(x, 0);

        line->setPen(pen);
        line->setSelectedPen(QPen(Qt::blue, 2, Qt::DashLine));
        line->setProperty("isEventLine", true);
        line->setLayer("overlay");

        return line;
    };

    QCPItemLine* lineTop = nullptr;
    QCPItemLine* lineBottom = nullptr;

    if (m_chartMode == Mode_Stacked) {
        // 分别在上下两个坐标系中创建线条，视觉上拼接成一条贯穿线
        lineTop = createLine(m_topRect);
        lineBottom = createLine(m_bottomRect);

        // 互联引用，用于同时选中
        if (lineTop && lineBottom) {
            QVariant vTop; vTop.setValue(reinterpret_cast<void*>(lineBottom));
            lineTop->setProperty("sibling", vTop);

            QVariant vBot; vBot.setValue(reinterpret_cast<void*>(lineTop));
            lineBottom->setProperty("sibling", vBot);
        }
    } else {
        // 单坐标系模式
        lineTop = createLine(m_plot->axisRect());
    }

    // 添加到管理列表
    if (lineTop) m_eventLines.append(lineTop);
    if (lineBottom) m_eventLines.append(lineBottom);

    m_plot->replot();
}

// ============================================================================
// 事件处理
// ============================================================================

void ChartWidget::onPlotMousePress(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        for (auto line : m_eventLines) {
            double dist = line->selectTest(event->pos(), false);
            if (dist >= 0 && dist < 10.0) {
                m_activeLine = line;
                QMenu menu(this);
                QAction* actSetting = menu.addAction("开/关井线设置...");
                connect(actSetting, &QAction::triggered, this, &ChartWidget::onEventLineSettingsTriggered);
                menu.exec(event->globalPosition().toPoint());
                return;
            }
        }

        if (m_chartMode == Mode_Stacked) {
            QMenu menu(this);
            QAction* actMoveX = menu.addAction("数据横向移动 (X Only)");
            connect(actMoveX, &QAction::triggered, this, &ChartWidget::onMoveDataXTriggered);
            QAction* actMoveY = menu.addAction("数据纵向移动 (Y Only)");
            connect(actMoveY, &QAction::triggered, this, &ChartWidget::onMoveDataYTriggered);
            menu.exec(event->globalPosition().toPoint());
            return;
        }
    }

    if (event->button() != Qt::LeftButton) return;

    if (checkHoverSplitLine(event->pos())) {
        m_interMode = Mode_Resizing_Axis;
        m_resizeStartY = event->pos().y();

        // [修复] 使用 rowStretchFactors().value()
        QList<double> factors = m_plot->plotLayout()->rowStretchFactors();
        double h1 = factors.value(1);
        double h2 = factors.value(2);

        if (h1 + h2 == 0) m_startTopRatio = 0.5;
        else m_startTopRatio = h1 / (h1 + h2);

        m_plot->setInteractions(QCP::Interaction(0));
        return;
    }

    if (m_interMode == Mode_Moving_Data_X || m_interMode == Mode_Moving_Data_Y) {
        QCPAxisRect* clickedRect = m_plot->axisRectAt(event->pos());
        if (clickedRect) {
            QList<QCPGraph*> graphs = clickedRect->graphs();
            if (!graphs.isEmpty()) {
                m_movingGraph = graphs.first();
                m_lastMoveDataPos = event->pos();
            } else {
                m_movingGraph = nullptr;
            }
        }
        return;
    }

    m_interMode = Mode_None; m_activeLine = nullptr; m_activeText = nullptr; m_activeArrow = nullptr; m_lastMousePos = event->pos();
    double tolerance = 8.0;

    for (int i = 0; i < m_plot->itemCount(); ++i) {
        if (auto text = qobject_cast<QCPItemText*>(m_plot->item(i))) {
            if (text->selectTest(event->pos(), false) < tolerance) {
                m_interMode = Mode_Dragging_Text; m_activeText = text;
                m_plot->deselectAll(); text->setSelected(true); m_plot->setInteractions(QCP::Interaction(0));
                m_plot->replot(); return;
            }
        }
    }

    for (auto line : m_eventLines) {
        if (line->selectTest(event->pos(), false) < tolerance) {
            m_plot->deselectAll();
            line->setSelected(true);
            QVariant v = line->property("sibling");
            if (v.isValid()) {
                QCPItemLine* sibling = static_cast<QCPItemLine*>(v.value<void*>());
                if (sibling) sibling->setSelected(true);
            }
            m_interMode = Mode_None;
            m_plot->replot();
            return;
        }
    }

    for (int i = 0; i < m_plot->itemCount(); ++i) {
        auto line = qobject_cast<QCPItemLine*>(m_plot->item(i));
        if (line && !line->property("isCharacteristic").isValid() && !line->property("isEventLine").isValid()) {
            double x1 = m_plot->xAxis->coordToPixel(line->start->coords().x()), y1 = m_plot->yAxis->coordToPixel(line->start->coords().y());
            double x2 = m_plot->xAxis->coordToPixel(line->end->coords().x()), y2 = m_plot->yAxis->coordToPixel(line->end->coords().y());
            QPointF p(event->pos());
            if (std::sqrt(pow(p.x()-x1,2)+pow(p.y()-y1,2)) < tolerance) { m_interMode=Mode_Dragging_ArrowStart; m_activeArrow=line; m_plot->setInteractions(QCP::Interaction(0)); return; }
            if (std::sqrt(pow(p.x()-x2,2)+pow(p.y()-y2,2)) < tolerance) { m_interMode=Mode_Dragging_ArrowEnd; m_activeArrow=line; m_plot->setInteractions(QCP::Interaction(0)); return; }
        }
    }

    for (int i = 0; i < m_plot->itemCount(); ++i) {
        QCPItemLine* line = qobject_cast<QCPItemLine*>(m_plot->item(i));
        if (!line || !line->property("isCharacteristic").isValid()) continue;
        double x1 = m_plot->xAxis->coordToPixel(line->start->coords().x()), y1 = m_plot->yAxis->coordToPixel(line->start->coords().y());
        double x2 = m_plot->xAxis->coordToPixel(line->end->coords().x()), y2 = m_plot->yAxis->coordToPixel(line->end->coords().y());
        QPointF p(event->pos());
        if (std::sqrt(pow(p.x()-x1,2)+pow(p.y()-y1,2)) < tolerance) { m_interMode=Mode_Dragging_Start; m_activeLine=line; }
        else if (std::sqrt(pow(p.x()-x2,2)+pow(p.y()-y2,2)) < tolerance) { m_interMode=Mode_Dragging_End; m_activeLine=line; }
        else if (distToSegment(p, QPointF(x1,y1), QPointF(x2,y2)) < tolerance) { m_interMode=Mode_Dragging_Line; m_activeLine=line; }

        if (m_interMode != Mode_None) { m_plot->deselectAll(); line->setSelected(true); m_plot->setInteractions(QCP::Interaction(0)); m_plot->replot(); return; }
    }

    m_plot->deselectAll();
    m_plot->replot();
}

void ChartWidget::onPlotMouseMove(QMouseEvent* event) {
    if (m_interMode == Mode_None) {
        if (checkHoverSplitLine(event->pos())) {
            m_plot->setCursor(Qt::SplitVCursor);
        } else {
            m_plot->setCursor(Qt::ArrowCursor);
        }
    }

    if (event->buttons() & Qt::LeftButton) {
        if (m_interMode == Mode_Resizing_Axis) {
            int totalHeight = m_topRect->rect().height() + m_bottomRect->rect().height();
            if (totalHeight > 0) {
                int dy = event->pos().y() - m_resizeStartY;
                double deltaRatio = (double)dy / totalHeight;
                double newTopRatio = qBound(0.1, m_startTopRatio + deltaRatio, 0.9);

                m_plot->plotLayout()->setRowStretchFactor(1, newTopRatio * 1000);
                m_plot->plotLayout()->setRowStretchFactor(2, (1.0 - newTopRatio) * 1000);
                m_plot->replot();
            }
            return;
        }

        QPointF currentPos = event->pos();
        QPointF delta = currentPos - m_lastMousePos;
        double mouseX = m_plot->xAxis->pixelToCoord(currentPos.x()), mouseY = m_plot->yAxis->pixelToCoord(currentPos.y());

        if ((m_interMode == Mode_Moving_Data_X || m_interMode == Mode_Moving_Data_Y) && m_movingGraph) {
            QCPAxis *xAxis = m_movingGraph->keyAxis();
            QCPAxis *yAxis = m_movingGraph->valueAxis();

            double dx = 0;
            double dy = 0;

            if (m_interMode == Mode_Moving_Data_X) {
                dx = xAxis->pixelToCoord(event->pos().x()) - xAxis->pixelToCoord(m_lastMoveDataPos.x());
            } else {
                dy = yAxis->pixelToCoord(event->pos().y()) - yAxis->pixelToCoord(m_lastMoveDataPos.y());
            }

            QSharedPointer<QCPGraphDataContainer> data = m_movingGraph->data();
            for (auto it = data->begin(); it != data->end(); ++it) {
                if (m_interMode == Mode_Moving_Data_X) it->key += dx;
                else it->value += dy;
            }

            if (m_interMode == Mode_Moving_Data_X && !m_eventLines.isEmpty()) {
                if (m_chartMode == Mode_Stacked && m_movingGraph->keyAxis()->axisRect() == m_bottomRect) {
                    for(auto line : m_eventLines) {
                        double currentX = line->start->coords().x();
                        double newX = currentX + dx;
                        line->start->setCoords(newX, line->start->coords().y());
                        line->end->setCoords(newX, line->end->coords().y());
                    }
                }
            }

            m_plot->replot();
            m_lastMoveDataPos = event->pos();
            return;
        }

        if (m_interMode == Mode_Dragging_Text && m_activeText) {
            double px = m_plot->xAxis->coordToPixel(m_activeText->position->coords().x()) + delta.x();
            double py = m_plot->yAxis->coordToPixel(m_activeText->position->coords().y()) + delta.y();
            m_activeText->position->setCoords(m_plot->xAxis->pixelToCoord(px), m_plot->yAxis->pixelToCoord(py));
        }
        else if (m_interMode == Mode_Dragging_ArrowStart && m_activeArrow) {
            if(m_activeArrow->start->parentAnchor()) m_activeArrow->start->setParentAnchor(nullptr);
            m_activeArrow->start->setCoords(mouseX, mouseY);
        } else if (m_interMode == Mode_Dragging_ArrowEnd && m_activeArrow) {
            if(m_activeArrow->end->parentAnchor()) m_activeArrow->end->setParentAnchor(nullptr);
            m_activeArrow->end->setCoords(mouseX, mouseY);
        }
        else if (m_interMode == Mode_Dragging_Line && m_activeLine) {
            double sPx = m_plot->xAxis->coordToPixel(m_activeLine->start->coords().x()) + delta.x();
            double sPy = m_plot->yAxis->coordToPixel(m_activeLine->start->coords().y()) + delta.y();
            double ePx = m_plot->xAxis->coordToPixel(m_activeLine->end->coords().x()) + delta.x();
            double ePy = m_plot->yAxis->coordToPixel(m_activeLine->end->coords().y()) + delta.y();
            m_activeLine->start->setCoords(m_plot->xAxis->pixelToCoord(sPx), m_plot->yAxis->pixelToCoord(sPy));
            m_activeLine->end->setCoords(m_plot->xAxis->pixelToCoord(ePx), m_plot->yAxis->pixelToCoord(ePy));

            if (m_annotations.contains(m_activeLine)) {
                auto note = m_annotations[m_activeLine];
                if (note.textItem) {
                    double tPx = m_plot->xAxis->coordToPixel(note.textItem->position->coords().x()) + delta.x();
                    double tPy = m_plot->yAxis->coordToPixel(note.textItem->position->coords().y()) + delta.y();
                    note.textItem->position->setCoords(m_plot->xAxis->pixelToCoord(tPx), m_plot->yAxis->pixelToCoord(tPy));
                }
            }
            updateAnnotationArrow(m_activeLine);
        }
        else if ((m_interMode == Mode_Dragging_Start || m_interMode == Mode_Dragging_End) && m_activeLine) {
            constrainLinePoint(m_activeLine, m_interMode == Mode_Dragging_Start, mouseX, mouseY);
        }

        m_lastMousePos = currentPos; m_plot->replot();
    }
}

void ChartWidget::onPlotMouseRelease(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_interMode == Mode_Moving_Data_X || m_interMode == Mode_Moving_Data_Y) {
        if (m_movingGraph) {
            emit graphDataModified(m_movingGraph);
        }
        m_movingGraph = nullptr;
    } else {
        if (m_interMode != Mode_None) {
            setZoomDragMode(Qt::Horizontal | Qt::Vertical);
        }
        m_interMode = Mode_None;
    }
}

void ChartWidget::onPlotMouseDoubleClick(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    for (int i = 0; i < m_plot->itemCount(); ++i) {
        if (auto text = qobject_cast<QCPItemText*>(m_plot->item(i))) {
            if (text->selectTest(event->pos(), false) < 10.0) { onEditItemRequested(text); return; }
        }
    }
}

// ============================================================================
// 辅助功能
// ============================================================================

void ChartWidget::setTitle(const QString &title) {
    refreshTitleElement();
    if (m_titleElement) {
        m_titleElement->setText(title);
        m_plot->replot();
    }
}

QString ChartWidget::title() const {
    if (m_titleElement) return m_titleElement->text();
    return QString();
}

void ChartWidget::refreshTitleElement() {
    m_titleElement = nullptr;
    if (m_plot->plotLayout()->elementCount() > 0) {
        if (auto el = qobject_cast<QCPTextElement*>(m_plot->plotLayout()->element(0, 0))) {
            m_titleElement = el;
            return;
        }
        for (int i = 0; i < m_plot->plotLayout()->elementCount(); ++i) {
            if (auto el = qobject_cast<QCPTextElement*>(m_plot->plotLayout()->elementAt(i))) {
                m_titleElement = el;
                return;
            }
        }
    }
}

MouseZoom *ChartWidget::getPlot() { return m_plot; }
void ChartWidget::setDataModel(QStandardItemModel *model) { m_dataModel = model; }

void ChartWidget::clearGraphs() {
    m_plot->clearGraphs();
    clearEventLines();
    m_plot->replot();
    exitMoveDataMode();
    setZoomDragMode(Qt::Horizontal | Qt::Vertical);
}

QCPAxisRect* ChartWidget::getTopRect() {
    if (m_chartMode == Mode_Single) return m_plot->axisRect();
    return m_topRect;
}
QCPAxisRect* ChartWidget::getBottomRect() {
    if (m_chartMode == Mode_Single) return nullptr;
    return m_bottomRect;
}

void ChartWidget::on_btnSavePic_clicked()
{
    QString dir = ModelParameter::instance()->getProjectPath();
    if (dir.isEmpty()) dir = QDir::currentPath();
    QString fileName = QFileDialog::getSaveFileName(this, "保存图片", dir + "/chart_export.png", "PNG (*.png);;JPG (*.jpg);;PDF (*.pdf)");
    if (fileName.isEmpty()) return;
    if (fileName.endsWith(".png")) m_plot->savePng(fileName);
    else if (fileName.endsWith(".jpg")) m_plot->saveJpg(fileName);
    else m_plot->savePdf(fileName);
}

void ChartWidget::on_btnExportData_clicked() { emit exportDataTriggered(); }

void ChartWidget::on_btnSetting_clicked() {
    refreshTitleElement();
    QString oldTitle;
    if (m_titleElement) oldTitle = m_titleElement->text();

    if (m_chartMode == Mode_Stacked) {
        ChartSetting2 dlg(m_plot, m_titleElement, m_topRect, m_bottomRect, this);
        dlg.exec();
    } else {
        ChartSetting1 dlg(m_plot, m_titleElement, this);
        dlg.exec();
    }

    refreshTitleElement();
    m_plot->replot();

    if (m_titleElement) {
        QString newTitle = m_titleElement->text();
        if (newTitle != oldTitle) {
            emit titleChanged(newTitle);
        }
    }
    emit graphsChanged();
}

void ChartWidget::on_btnReset_clicked() {
    m_plot->rescaleAxes();
    setZoomDragMode(Qt::Horizontal | Qt::Vertical);
    if(m_plot->xAxis->scaleType()==QCPAxis::stLogarithmic && m_plot->xAxis->range().lower<=0) m_plot->xAxis->setRangeLower(1e-3);
    if(m_plot->yAxis->scaleType()==QCPAxis::stLogarithmic && m_plot->yAxis->range().lower<=0) m_plot->yAxis->setRangeLower(1e-3);
    m_plot->replot();
}
void ChartWidget::on_btnDrawLine_clicked() { m_lineMenu->exec(ui->btnDrawLine->mapToGlobal(QPoint(0, ui->btnDrawLine->height()))); }

void ChartWidget::addCharacteristicLine(double slope) {
    QCPAxisRect* rect = (m_chartMode == Mode_Stacked && m_topRect) ? m_topRect : m_plot->axisRect();
    double lowerX = rect->axis(QCPAxis::atBottom)->range().lower;
    double upperX = rect->axis(QCPAxis::atBottom)->range().upper;
    double lowerY = rect->axis(QCPAxis::atLeft)->range().lower;
    double upperY = rect->axis(QCPAxis::atLeft)->range().upper;

    bool isLogX = (rect->axis(QCPAxis::atBottom)->scaleType() == QCPAxis::stLogarithmic);
    bool isLogY = (rect->axis(QCPAxis::atLeft)->scaleType() == QCPAxis::stLogarithmic);

    double centerX = isLogX ? pow(10, (log10(lowerX) + log10(upperX)) / 2.0) : (lowerX + upperX) / 2.0;
    double centerY = isLogY ? pow(10, (log10(lowerY) + log10(upperY)) / 2.0) : (lowerY + upperY) / 2.0;

    double x1, y1, x2, y2;
    calculateLinePoints(slope, centerX, centerY, x1, y1, x2, y2, isLogX, isLogY);

    QCPItemLine* line = new QCPItemLine(m_plot);
    line->setClipAxisRect(rect);
    line->start->setCoords(x1, y1);
    line->end->setCoords(x2, y2);
    QPen pen(Qt::black, 2, Qt::DashLine);
    line->setPen(pen);
    line->setSelectedPen(QPen(Qt::blue, 2, Qt::SolidLine));
    line->setProperty("fixedSlope", slope);
    line->setProperty("isLogLog", (isLogX && isLogY));
    line->setProperty("isCharacteristic", true);
    m_plot->replot();
}

void ChartWidget::calculateLinePoints(double slope, double centerX, double centerY, double& x1, double& y1, double& x2, double& y2, bool isLogX, bool isLogY) {
    if (isLogX && isLogY) {
        double span = 3.0;
        x1 = centerX / span; x2 = centerX * span;
        y1 = centerY * pow(x1 / centerX, slope); y2 = centerY * pow(x2 / centerX, slope);
    } else {
        QCPAxisRect* rect = m_plot->axisRect();
        x1 = rect->axis(QCPAxis::atBottom)->range().lower;
        x2 = rect->axis(QCPAxis::atBottom)->range().upper;
        y1 = centerY; y2 = centerY;
    }
}

double ChartWidget::distToSegment(const QPointF& p, const QPointF& s, const QPointF& e) {
    double l2 = (s.x()-e.x())*(s.x()-e.x()) + (s.y()-e.y())*(s.y()-e.y());
    if (l2 == 0) return std::sqrt((p.x()-s.x())*(p.x()-s.x()) + (p.y()-s.y())*(p.y()-s.y()));
    double t = ((p.x()-s.x())*(e.x()-s.x()) + (p.y()-s.y())*(e.y()-s.y())) / l2;
    t = std::max(0.0, std::min(1.0, t));
    QPointF proj = s + t * (e - s);
    return std::sqrt((p.x()-proj.x())*(p.x()-proj.x()) + (p.y()-proj.y())*(p.y()-proj.y()));
}

void ChartWidget::onEventLineSettingsTriggered() {
    if (!m_activeLine) return;

    StyleSelectorDialog dlg(StyleSelectorDialog::ModeLine, this);
    dlg.setWindowTitle("开/关井线设置");
    dlg.setPen(m_activeLine->pen());

    if (dlg.exec() == QDialog::Accepted) {
        QPen newPen = dlg.getPen();
        m_activeLine->setPen(newPen);

        QVariant v = m_activeLine->property("sibling");
        if (v.isValid()) {
            QCPItemLine* sibling = static_cast<QCPItemLine*>(v.value<void*>());
            if (sibling) sibling->setPen(newPen);
        }
        m_plot->replot();
    }
}

void ChartWidget::onMoveDataXTriggered() {
    m_interMode = Mode_Moving_Data_X;
    m_plot->setInteractions(QCP::Interaction(0));
    m_plot->setCursor(Qt::SizeHorCursor);
    QMessageBox::information(this, "提示", "已进入横向数据移动模式。\n按 ESC 键退出此模式。");
    m_plot->setFocus();
    this->setFocus();
}

void ChartWidget::onMoveDataYTriggered() {
    m_interMode = Mode_Moving_Data_Y;
    m_plot->setInteractions(QCP::Interaction(0));
    m_plot->setCursor(Qt::SizeVerCursor);
    QMessageBox::information(this, "提示", "已进入纵向数据移动模式。\n按 ESC 键退出此模式。");
    m_plot->setFocus();
    this->setFocus();
}

void ChartWidget::onZoomHorizontalTriggered() { setZoomDragMode(Qt::Horizontal); }
void ChartWidget::onZoomVerticalTriggered() { setZoomDragMode(Qt::Vertical); }
void ChartWidget::onZoomDefaultTriggered() { setZoomDragMode(Qt::Horizontal | Qt::Vertical); }

void ChartWidget::setZoomDragMode(Qt::Orientations orientations) {
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectItems);

    auto configureRect = [&](QCPAxisRect* rect) {
        if(!rect) return;
        rect->setRangeDrag(orientations);
        rect->setRangeZoom(orientations);

        QCPAxis *hAxis = (orientations & Qt::Horizontal) ? rect->axis(QCPAxis::atBottom) : nullptr;
        QCPAxis *vAxis = (orientations & Qt::Vertical) ? rect->axis(QCPAxis::atLeft) : nullptr;

        rect->setRangeDragAxes(hAxis, vAxis);
        rect->setRangeZoomAxes(hAxis, vAxis);
    };

    if (m_chartMode == Mode_Stacked) {
        configureRect(m_topRect);
        configureRect(m_bottomRect);
    } else {
        configureRect(m_plot->axisRect());
    }
}

void ChartWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && (m_interMode == Mode_Moving_Data_X || m_interMode == Mode_Moving_Data_Y)) {
        exitMoveDataMode();
    }
    QWidget::keyPressEvent(event);
}

void ChartWidget::exitMoveDataMode() {
    if (m_interMode == Mode_Moving_Data_X || m_interMode == Mode_Moving_Data_Y) {
        m_interMode = Mode_None;
        m_movingGraph = nullptr;
        m_plot->setCursor(Qt::ArrowCursor);
        setZoomDragMode(Qt::Horizontal | Qt::Vertical);
    }
}

void ChartWidget::constrainLinePoint(QCPItemLine* line, bool isMovingStart, double mouseX, double mouseY) {
    double k = line->property("fixedSlope").toDouble();
    bool isLogLog = line->property("isLogLog").toBool();
    double xFixed = isMovingStart ? line->end->coords().x() : line->start->coords().x();
    double yFixed = isMovingStart ? line->end->coords().y() : line->start->coords().y();
    double yNew;
    if (isLogLog) {
        if (xFixed <= 0) xFixed = 1e-5;
        if (mouseX <= 0) mouseX = 1e-5;
        yNew = yFixed * pow(mouseX / xFixed, k);
    } else {
        QCPAxisRect* rect = m_plot->axisRect();
        double scale = rect->axis(QCPAxis::atLeft)->range().size() / rect->axis(QCPAxis::atBottom)->range().size();
        yNew = yFixed + (k * scale) * (mouseX - xFixed);
    }
    if (isMovingStart) line->start->setCoords(mouseX, yNew); else line->end->setCoords(mouseX, yNew);
}

void ChartWidget::updateAnnotationArrow(QCPItemLine* line) {
    if (m_annotations.contains(line)) {
        ChartAnnotation note = m_annotations[line];
        double midX = (line->start->coords().x() + line->end->coords().x()) / 2.0;
        double midY = (line->start->coords().y() + line->end->coords().y()) / 2.0;
        if(note.arrowItem) note.arrowItem->end->setCoords(midX, midY);
    }
}

void ChartWidget::onAddAnnotationRequested(QCPItemLine* line) { addAnnotationToLine(line); }
void ChartWidget::onDeleteSelectedRequested() { deleteSelectedItems(); }

void ChartWidget::onLineStyleRequested(QCPItemLine* line) {
    if (!line) return;
    StyleSelectorDialog dlg(StyleSelectorDialog::ModeLine, this);
    dlg.setWindowTitle("标识线样式设置");
    dlg.setPen(line->pen());
    if (dlg.exec() == QDialog::Accepted) {
        line->setPen(dlg.getPen());
        m_plot->replot();
    }
}

void ChartWidget::onEditItemRequested(QCPAbstractItem* item) {
    if (auto text = qobject_cast<QCPItemText*>(item)) {
        bool ok;
        QString newContent = QInputDialog::getText(this, "修改标注", "内容:", QLineEdit::Normal, text->text(), &ok);
        if (ok && !newContent.isEmpty()) { text->setText(newContent); m_plot->replot(); }
    }
}

void ChartWidget::addAnnotationToLine(QCPItemLine* line) {
    if (!line) return;
    if (m_annotations.contains(line)) {
        ChartAnnotation old = m_annotations.take(line);
        if(old.textItem) m_plot->removeItem(old.textItem);
        if(old.arrowItem) m_plot->removeItem(old.arrowItem);
    }
    double k = line->property("fixedSlope").toDouble();
    bool ok;
    QString text = QInputDialog::getText(this, "添加标注", "输入:", QLineEdit::Normal, QString("k=%1").arg(k), &ok);
    if (!ok || text.isEmpty()) return;

    QCPItemText* txt = new QCPItemText(m_plot);
    txt->setText(text);
    txt->position->setType(QCPItemPosition::ptPlotCoords);
    double midX = (line->start->coords().x() + line->end->coords().x()) / 2.0;
    double midY = (line->start->coords().y() + line->end->coords().y()) / 2.0;
    txt->position->setCoords(midX, midY * 1.5);

    QCPItemLine* arr = new QCPItemLine(m_plot);
    arr->setHead(QCPLineEnding::esSpikeArrow);
    arr->start->setParentAnchor(txt->bottom);
    arr->end->setCoords(midX, midY);

    ChartAnnotation note; note.textItem = txt; note.arrowItem = arr;
    m_annotations.insert(line, note);
    m_plot->replot();
}

void ChartWidget::deleteSelectedItems() {
    auto items = m_plot->selectedItems();
    for (auto item : items) {
        m_plot->removeItem(item);
    }
    m_plot->replot();
}
