/*
 * 文件名: chartsetting2.cpp
 * 文件作用: 双坐标图表设置对话框实现文件
 * 功能描述:
 * 1. 初始化对话框，加载双坐标系的各项参数 (压力轴/产量轴)。
 * 2. 实现了曲线列表管理功能：
 * - 样式列：加粗显示的线型和点型预览，居中。
 * - 名称列：可编辑。
 * - 图例显示列：复选框居中，根据曲线位置控制其在顶部或底部图例中的显示。
 */

#include "chartsetting2.h"
#include "ui_chartsetting2.h"
#include "chartwidget.h" // 必须引用以获取 getBottomLegend
#include <QDebug>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QLineF>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>

// 构造函数
ChartSetting2::ChartSetting2(MouseZoom* plot, QCPTextElement* title,
                             QCPAxisRect* topRect, QCPAxisRect* bottomRect,
                             QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChartSetting2),
    m_plot(plot),
    m_title(title),
    m_topRect(topRect),
    m_bottomRect(bottomRect)
{
    ui->setupUi(this);
    this->setWindowTitle("图表设置 (双坐标)");

    // 设置表格列数和表头
    ui->tableGraphs->setColumnCount(3);
    QStringList headers;
    headers << "样式" << "曲线名称" << "图例显示";
    ui->tableGraphs->setHorizontalHeaderLabels(headers);

    // 设置列宽模式
    ui->tableGraphs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableGraphs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableGraphs->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    initData();
}

// 析构函数
ChartSetting2::~ChartSetting2()
{
    delete ui;
}

// 初始化数据
void ChartSetting2::initData()
{
    if (!m_plot) return;

    // --- 1. 标题设置 ---
    if (m_title) {
        ui->editTitle->setText(m_title->text());
        ui->checkTitleVisible->setChecked(m_title->visible());
    }

    // --- 2. 顶部坐标轴 (Top Rect) ---
    if (m_topRect) {
        QCPAxis *x = m_topRect->axis(QCPAxis::atBottom);
        QCPAxis *y = m_topRect->axis(QCPAxis::atLeft);

        ui->editTopYLabel->setText(y->label());
        ui->spinTopYMin->setValue(y->range().lower);
        ui->spinTopYMax->setValue(y->range().upper);

        bool isLogY = (y->scaleType() == QCPAxis::stLogarithmic);
        ui->checkTopYLog->setChecked(isLogY);

        if(x->grid()) ui->checkTopXGrid->setChecked(x->grid()->visible());
        if(y->grid()) ui->checkTopYGrid->setChecked(y->grid()->visible());
    } else {
        ui->tabTop->setEnabled(false);
    }

    // --- 3. 底部坐标轴 (Bottom Rect) ---
    if (m_bottomRect) {
        QCPAxis *x = m_bottomRect->axis(QCPAxis::atBottom);
        QCPAxis *y = m_bottomRect->axis(QCPAxis::atLeft);

        ui->editBottomXLabel->setText(x->label());
        ui->editBottomYLabel->setText(y->label());
        ui->spinBottomXMin->setValue(x->range().lower);
        ui->spinBottomXMax->setValue(x->range().upper);
        ui->spinBottomYMin->setValue(y->range().lower);
        ui->spinBottomYMax->setValue(y->range().upper);

        if(x->grid()) ui->checkBottomXGrid->setChecked(x->grid()->visible());
        if(y->grid()) ui->checkBottomYGrid->setChecked(y->grid()->visible());
    } else {
        ui->tabBottom->setEnabled(false);
    }

    // --- 4. 曲线列表 ---
    int graphCount = m_plot->graphCount();
    ui->tableGraphs->setRowCount(graphCount);

    // 获取 ChartWidget 指针以访问 bottomLegend
    ChartWidget* chartWidget = qobject_cast<ChartWidget*>(parent());
    QCPLegend* bottomLegend = chartWidget ? chartWidget->getBottomLegend() : nullptr;

    for(int i = 0; i < graphCount; ++i) {
        QCPGraph *graph = m_plot->graph(i);
        if(!graph) continue;

        // --- 第一列：样式预览 (加粗、居中) ---
        QPixmap pix(60, 20);
        pix.fill(Qt::transparent);
        QCPPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);

        QPen thickPen = graph->pen();
        if (thickPen.style() != Qt::NoPen) {
            thickPen.setWidthF(std::max(thickPen.widthF(), 1.0) + 2.5);
            painter.setPen(thickPen);
            painter.drawLine(QLineF(0, 10, 60, 10));
        }

        if (graph->scatterStyle().shape() != QCPScatterStyle::ssNone) {
            QCPScatterStyle scatter = graph->scatterStyle();
            if (scatter.pen().style() != Qt::NoPen) {
                QPen sp = scatter.pen();
                sp.setWidthF(std::max(sp.widthF(), 1.0) + 2.5);
                scatter.setPen(sp);
            }
            scatter.applyTo(&painter, thickPen);
            scatter.drawShape(&painter, 30, 10);
        }

        QWidget *styleWidget = new QWidget();
        QHBoxLayout *styleLayout = new QHBoxLayout(styleWidget);
        styleLayout->setContentsMargins(0, 0, 0, 0);
        styleLayout->setAlignment(Qt::AlignCenter);
        QLabel *styleLabel = new QLabel();
        styleLabel->setPixmap(pix);
        styleLayout->addWidget(styleLabel);
        ui->tableGraphs->setCellWidget(i, 0, styleWidget);

        // --- 第二列：名称 ---
        QTableWidgetItem* nameItem = new QTableWidgetItem(graph->name());
        ui->tableGraphs->setItem(i, 1, nameItem);

        // --- 第三列：图例显示 (复选框居中) ---
        QWidget *checkWidget = new QWidget();
        QHBoxLayout *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);

        QCheckBox *pCheckBox = new QCheckBox();

        // [关键] 根据曲线所属坐标轴判断查找哪个图例
        bool inLegend = false;
        if (m_topRect && graph->keyAxis()->axisRect() == m_topRect) {
            inLegend = (m_plot->legend && m_plot->legend->itemWithPlottable(graph) != nullptr);
        } else if (m_bottomRect && graph->keyAxis()->axisRect() == m_bottomRect) {
            inLegend = (bottomLegend && bottomLegend->itemWithPlottable(graph) != nullptr);
        }

        pCheckBox->setChecked(inLegend);

        checkLayout->addWidget(pCheckBox);
        ui->tableGraphs->setCellWidget(i, 2, checkWidget);
    }
}

// 应用设置
void ChartSetting2::applySettings()
{
    if (!m_plot) return;

    // 应用标题
    if (m_title) {
        m_title->setText(ui->editTitle->text());
        m_title->setVisible(ui->checkTitleVisible->isChecked());
    }

    // 应用顶部坐标轴
    if (m_topRect) {
        QCPAxis *x = m_topRect->axis(QCPAxis::atBottom);
        QCPAxis *y = m_topRect->axis(QCPAxis::atLeft);
        y->setLabel(ui->editTopYLabel->text());
        y->setRange(ui->spinTopYMin->value(), ui->spinTopYMax->value());

        if (ui->checkTopYLog->isChecked()) {
            y->setScaleType(QCPAxis::stLogarithmic);
            y->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
        } else {
            y->setScaleType(QCPAxis::stLinear);
            y->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
        }

        if(x->grid()) x->grid()->setVisible(ui->checkTopXGrid->isChecked());
        if(y->grid()) y->grid()->setVisible(ui->checkTopYGrid->isChecked());
    }

    // 应用底部坐标轴
    if (m_bottomRect) {
        QCPAxis *x = m_bottomRect->axis(QCPAxis::atBottom);
        QCPAxis *y = m_bottomRect->axis(QCPAxis::atLeft);
        x->setLabel(ui->editBottomXLabel->text());
        y->setLabel(ui->editBottomYLabel->text());
        x->setRange(ui->spinBottomXMin->value(), ui->spinBottomXMax->value());
        y->setRange(ui->spinBottomYMin->value(), ui->spinBottomYMax->value());

        if(x->grid()) x->grid()->setVisible(ui->checkBottomXGrid->isChecked());
        if(y->grid()) y->grid()->setVisible(ui->checkBottomYGrid->isChecked());
    }

    // 应用曲线列表设置
    ChartWidget* chartWidget = qobject_cast<ChartWidget*>(parent());
    QCPLegend* bottomLegend = chartWidget ? chartWidget->getBottomLegend() : nullptr;

    int graphCount = m_plot->graphCount();
    if(ui->tableGraphs->rowCount() == graphCount) {
        for(int i = 0; i < graphCount; ++i) {
            QCPGraph *graph = m_plot->graph(i);
            if (!graph) continue;

            QTableWidgetItem* nameItem = ui->tableGraphs->item(i, 1);
            if(nameItem) graph->setName(nameItem->text());

            QWidget *checkWidget = ui->tableGraphs->cellWidget(i, 2);
            if (checkWidget) {
                QCheckBox *pCheckBox = checkWidget->findChild<QCheckBox*>();
                if (pCheckBox) {
                    bool showLegend = pCheckBox->isChecked();

                    // [关键] 确定目标图例
                    QCPLegend* targetLegend = nullptr;
                    if (m_topRect && graph->keyAxis()->axisRect() == m_topRect) targetLegend = m_plot->legend;
                    else if (m_bottomRect && graph->keyAxis()->axisRect() == m_bottomRect) targetLegend = bottomLegend;

                    if (targetLegend) {
                        bool currentlyInLegend = (targetLegend->itemWithPlottable(graph) != nullptr);
                        if (showLegend && !currentlyInLegend) {
                            graph->addToLegend(targetLegend);
                        } else if (!showLegend && currentlyInLegend) {
                            graph->removeFromLegend(targetLegend);
                        }
                    }
                }
            }
        }
    }

    m_plot->replot();
}

void ChartSetting2::on_btnOk_clicked()
{
    applySettings();
    accept();
}

void ChartSetting2::on_btnCancel_clicked()
{
    reject();
}

void ChartSetting2::on_btnApply_clicked()
{
    applySettings();
}
