/*
 * 文件名: chartwidget.h
 * 文件作用: 通用图表组件头文件
 * 功能描述:
 * 1. 封装 QCustomPlot 基础功能，提供统一的绘图接口。
 * 2. 支持单坐标系 (Mode_Single) 和双坐标系叠加 (Mode_Stacked) 两种模式。
 * 3. 支持双图例管理 (m_legendBottom) 及上下坐标系高度自由调整。
 * 4. [优化] addEventLine 支持在双坐标系模式下绘制贯穿上下的事件线。
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QMenu>
#include <QStandardItemModel>
#include <QMap>
#include "mousezoom.h"
#include "qcustomplot.h"

// 标注结构体
struct ChartAnnotation {
    QCPItemText* textItem;
    QCPItemLine* arrowItem;
};

namespace Ui {
class ChartWidget;
}

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    // 图表显示模式枚举
    enum ChartMode {
        Mode_Single,   // 单坐标系模式 (默认)
        Mode_Stacked   // 双坐标系叠加模式 (压力+产量)
    };

    // 交互模式枚举
    enum InteractionMode {
        Mode_None,
        Mode_Dragging_Text,       // 拖动文本标注
        Mode_Dragging_ArrowStart, // 拖动箭头起点
        Mode_Dragging_ArrowEnd,   // 拖动箭头终点
        Mode_Dragging_Line,       // 拖动线条整体
        Mode_Dragging_Start,      // 拖动线条起点
        Mode_Dragging_End,        // 拖动线条终点
        Mode_Moving_Data_X,       // 数据横向平移
        Mode_Moving_Data_Y,       // 数据纵向平移
        Mode_Resizing_Axis        // 调整上下坐标系高度比例
    };

    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();

    // 获取内部 QCustomPlot 对象
    MouseZoom* getPlot();

    // 设置/获取标题
    void setTitle(const QString& title);
    QString title() const;

    // 设置图表模式
    void setChartMode(ChartMode mode);
    ChartMode getChartMode() const;

    // 获取坐标轴矩形指针
    QCPAxisRect* getTopRect();
    QCPAxisRect* getBottomRect();

    // 获取底部图例指针
    QCPLegend* getBottomLegend() { return m_legendBottom; }

    // 数据清理与设置
    void setDataModel(QStandardItemModel* model);
    void clearGraphs();

    // 事件线 (开/关井线) 操作
    // [优化] 在双坐标模式下会绘制贯穿线
    void addEventLine(double x, int type); // type: 0=红线(关), 1=绿线(开)
    void clearEventLines();

signals:
    void exportDataRequested();
    void exportDataTriggered();
    void titleChanged(const QString& newTitle);
    void graphDataModified(QCPGraph* graph);
    void graphsChanged(); // 图例或曲线变化信号

private slots:
    // 工具栏按钮槽函数
    void on_btnSavePic_clicked();
    void on_btnExportData_clicked();
    void on_btnSetting_clicked();
    void on_btnReset_clicked();
    void on_btnDrawLine_clicked();

    // 右键菜单与交互槽函数
    void addCharacteristicLine(double slope);
    void onPlotMousePress(QMouseEvent* event);
    void onPlotMouseMove(QMouseEvent* event);
    void onPlotMouseRelease(QMouseEvent* event);
    void onPlotMouseDoubleClick(QMouseEvent* event);

    // 特征线与标注相关
    void onAddAnnotationRequested(QCPItemLine* line);
    void onLineStyleRequested(QCPItemLine* line);
    void onDeleteSelectedRequested();
    void onEditItemRequested(QCPAbstractItem* item);
    void onEventLineSettingsTriggered();

    // 数据移动相关
    void onMoveDataXTriggered();
    void onMoveDataYTriggered();

    // 缩放控制
    void onZoomHorizontalTriggered();
    void onZoomVerticalTriggered();
    void onZoomDefaultTriggered();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Ui::ChartWidget *ui;
    MouseZoom* m_plot;
    QStandardItemModel* m_dataModel;
    QCPTextElement* m_titleElement;

    ChartMode m_chartMode;
    QCPAxisRect* m_topRect;    // 顶部坐标系 (或单图模式下的唯一坐标系)
    QCPAxisRect* m_bottomRect; // 底部坐标系 (仅叠加模式有效)

    // 底部图例与边距组
    QCPLegend* m_legendBottom;
    QCPMarginGroup* m_marginGroup;

    // 交互状态
    InteractionMode m_interMode;
    QPointF m_lastMousePos;
    QPointF m_lastMoveDataPos;
    QCPItemLine* m_activeLine;
    QCPItemText* m_activeText;
    QCPItemLine* m_activeArrow;
    QCPGraph* m_movingGraph;

    // 调整高度交互变量
    int m_resizeStartY;
    double m_startTopRatio;

    QMenu* m_lineMenu;
    QList<QCPItemLine*> m_eventLines;
    QMap<QCPItemLine*, ChartAnnotation> m_annotations;

    // 初始化函数
    void initUi();
    void initConnections();
    void setupAxisRect(QCPAxisRect* rect);
    void refreshTitleElement();

    // 辅助函数
    void calculateLinePoints(double slope, double centerX, double centerY, double& x1, double& y1, double& x2, double& y2, bool isLogX, bool isLogY);
    double distToSegment(const QPointF& p, const QPointF& s, const QPointF& e);
    void constrainLinePoint(QCPItemLine* line, bool isMovingStart, double mouseX, double mouseY);
    void updateAnnotationArrow(QCPItemLine* line);
    void addAnnotationToLine(QCPItemLine* line);
    void deleteSelectedItems();
    void setZoomDragMode(Qt::Orientations orientations);
    void exitMoveDataMode();

    // 检查鼠标是否在分割线上
    bool checkHoverSplitLine(const QPoint& pos);
};

#endif // CHARTWIDGET_H
