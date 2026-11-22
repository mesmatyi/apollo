# User Guide - Web Application Performance Monitor and Controller

## 1. Introduction
The Web Application Performance Monitor and Controller is an important component of Dreamview. It monitors Dreamview's performance metrics and provides some performance regulation features.

## 2. Usage
```typescript
import LogicController from './LogicController';
import PerformanceMonitor from './PerformanceMonitor';

const monitor = new PerformanceMonitor();
export const performanceController = new LogicController(monitor, {
    highLoadThreshold: 30,
    sampleInterval: 1000,
});

class DataProcessingModule {
    private isHighLoad = false;

    constructor(private performanceController: LogicController) {
        this.performanceController.monitor.fps$.subscribe(fps => {
            this.adjustPerformance(fps < 30);
        });
    }

    start() {
        console.log("Data processing module started");
        this.processData();
    }

    private processData() {
        if (!this.isHighLoad) {
            console.log("Running complex data processing");
            // Complex data processing logic
        } else {
            console.log("Running simplified data processing");
            // Simplified data processing logic
        }

        setTimeout(() => this.processData(), this.isHighLoad ? 2000 : 1000);
    }

    adjustPerformance(highLoad: boolean) {
        this.isHighLoad = highLoad;
        console.log(`Data processing module has ${highLoad ? 'decreased' : 'increased'} its load`);
    }
}


const dataModule = new DataProcessingModule(performanceController);
dataModule.start();
```
