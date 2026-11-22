
# Panel

## Summary
The Panel component is the container of all panels in Dreamview. It provides each panel with data subscription (subscription with a Channel and subscription without a Channel), adding a subscribed Channel, updating a subscribed Channel, unsubscribing, panel full screen, shortcut key registration, panel size listening, and other related information.

## Data Subscription
Panel data subscription includes the panel's initial subscription, adding or updating a Channel subscription, and unsubscribing.

### Panel Initial Subscription
The code related to the panel's initial subscription is as follows:
```TypeScript
useEffect(() => {
    if (!isMainConnected || metadata.length <= 0) return noop;
    if (subscribeInfo) {
        const len = subscribeInfo?.length;
        // const obs: CountedSubject<unknown>[] = [];
        for (let i = 0; i < len; i += 1) {
            const curSubcibe = subscribeInfo[i];
            const name = curSubcibe?.name;
            const curItem = metadata.find((item) => item.dataName === name);
            if (!curItem) {
                // eslint-disable-next-line no-continue
                continue;
            }

            const connectedSubj: CountedSubject<unknown> = handleSubscribe(curSubcibe);
            // 2. If it contains a channel => subcribeToDataWithChannel => obs.push
            if (connectedSubj) {
                connectedSubjectsRef.current.push({
                    name,
                    connectedSubject: connectedSubj,
                });
            }
        }
        if (!hasSubscribed) {
            setHasSubscribed(true);
        }

        // eslint-disable-next-line no-restricted-syntax
        for (const subject of connectedSubjectsRef.current) {
            const subscription = subject.connectedSubject.subscribe((val) => {
                checkDataEmpty(val);

                if (initSubscriptionMapRef.current && !isEmpty(initSubscriptionMapRef.current)) {
                    const consumer = initSubscriptionMapRef.current[subject.name].consumer ?? defaultConsumer;
                    consumer(val);
                }
            });

            subscriptionsRef.current.push({
                name: subject.name,
                subscription,
            });
        }
    }

    return () => {
        connectedSubjectsRef.current = [];
        subscriptionsRef.current.forEach((subscription) => {
            subscription.subscription.unsubscribe();
        });
    };
}, [metadata, isMainConnected]);
```

This useEffect handles the subscription information passed in when the panel is initialized. The main process is as follows:
1. Traverse the subscription information subscribeInfo and filter out the subscription information whose name does not exist in metadata
2. Call handleSubscribe to handle the data subscription. handleSubscribe can handle different subscription types: subscription without a Channel, subscription with an explicit Channel name, and subscription where the Channel is default
3. Store the CountedSubject obtained from handleSubscribe in connectedSubjectsRef
4. Traverse connectedSubjectsRef and declare, for each CountedSubject, the corresponding subscription function provided by the panel, completing the initial data subscription

### Adding a Subscribed Channel & Updating a Subscribed Channel

The main logic for adding a Channel and updating a Channel is in the addChannel function:
```TypeScript
const addChannel = useCallback(
    (newChannelInfo: SubscribeInfo) => {
        if (isMainConnected) {
            const curSubjects = connectedSubjectsRef.current;
            const targetSubjectIndex = curSubjects.findIndex(
                (curSubject) => curSubject.name === newChannelInfo.name,
            );
            subscriptionsRef.current.forEach((subscription) => {
                subscription.subscription.unsubscribe();
            });

            if (targetSubjectIndex === -1) {
                // Does not exist => add a new subscription
                const newConnectedSubj: CountedSubject<unknown> = handleSubscribe(newChannelInfo);
                if (newConnectedSubj) {
                    const subscription = newConnectedSubj.subscribe((val) => {
                        checkDataEmpty(val);

                        if (initSubscriptionMapRef.current && !isEmpty(initSubscriptionMapRef.current)) {
                            const consumer =
                                initSubscriptionMapRef.current[newChannelInfo.name]?.consumer ??
                                defaultConsumer;
                            consumer(val);
                        }
                    });

                    curSubjects.push({
                        name: newChannelInfo.name,
                        connectedSubject: newConnectedSubj,
                    });

                    subscriptionsRef.current.push({
                        name: newChannelInfo.name,
                        subscription,
                    });
                }
            } else {
                // Exists => resubscribe
                const subscriptionIndex = subscriptionsRef.current.findIndex(
                    (subscription) => subscription.name === newChannelInfo.name,
                );
                const curSubscription = subscriptionsRef.current[subscriptionIndex];

                curSubscription.subscription.unsubscribe();

                const newConnectedSubj: CountedSubject<unknown> = handleSubscribe(newChannelInfo);
                if (newConnectedSubj) {
                    const newSubscription = newConnectedSubj.subscribe((val) => {
                        checkDataEmpty(val);

                        if (initSubscriptionMapRef.current && !isEmpty(initSubscriptionMapRef.current)) {
                            const consumer =
                                initSubscriptionMapRef.current[newChannelInfo.name]?.consumer ??
                                defaultConsumer;
                            consumer(val);
                        }
                    });

                    // 2.2 Replace the corresponding subject in the connectedSubjectsRef.current array
                    curSubjects[targetSubjectIndex] = {
                        name: newChannelInfo.name,
                        connectedSubject: newConnectedSubj,
                    };

                    subscriptionsRef.current[subscriptionIndex] = {
                        name: newChannelInfo.name,
                        subscription: newSubscription,
                    };
                }
            }
        }
    },
    [isMainConnected],
);
```

addChannel can handle adding and updating Channel information. The main flow is as follows:
1. Traverse connectedSubjectsRef to find whether the name corresponding to the current new Channel is already subscribed
2. If it is not subscribed, call handleSubscribe to add a new subscription; if it is already subscribed, resubscribe

addChannel is used in two places in Panel:
```TypeScript
useNotifyInitialChanel(panelId, addChannel);
useUpdateChannel(panelId, addChannel);
```
useNotifyInitialChanel is used for the ChannelSlector (Channel drop-down selector) to initially select the default Channel
useUpdateChannel is used to update the Channel

### Unsubscribing
The unsubscribe logic is in closeSubcription:

```TypeScript
const closeSubcription = useCallback(
    (name: string) => {
        const targetSubjectIndex = connectedSubjectsRef.current.findIndex(
            (curSubject) => curSubject.name === name,
        );

        if (targetSubjectIndex !== -1) {
            const subscriptionIndex = subscriptionsRef.current.findIndex(
                (subscription) => subscription.name === name,
            );
            const curSubscription = subscriptionsRef.current[subscriptionIndex];

            curSubscription.subscription.unsubscribe();

            subscriptionsRef.current = subscriptionsRef.current.filter(
                (subscriptionInfo) => subscriptionInfo.name !== name,
            );
            connectedSubjectsRef.current = connectedSubjectsRef.current.filter(
                (connectedSubjectInfo) => connectedSubjectInfo.name !== name,
            );
        }
    },
    [isMainConnected],
);
```
The unsubscribe logic is as follows:
1. Find the name to unsubscribe in connectedSubjectsRef, and if it exists, perform the cancellation
2. Find the corresponding subscription and execute curSubscription.subscription.unsubscribe();
3. Update subscriptionsRef (which stores the subscribed subscription[]) and connectedSubjectsRef (which stores the subscribed Subject[])

## Panel Size Listening

The panel full-screen feature is implemented based on react-resize-detector, and panel developers can use onPanelResize to listen to the panel's width and height:
```TypeScript
const { ref: panelRootRef } = useResizeDetector({
    onResize: (width: number, height: number) => {
        panelWidthRef.current = width;
        if (resizeCallBackRef.current) {
            resizeCallBackRef.current(width, height);
        }
        resizeCallBackArrRef.current.forEach((cb) => {
            try {
                cb(width, height);
            } catch (err) {
                console.log(err);
            }
        });
    },
});

const onPanelResize = useCallback((onResize: OnResizeCallback) => {
    resizeCallBackRef.current = onResize;
    resizeCallBackArrRef.current.push(onResize);
    if (panelWidthRef.current !== -1) {
        onResize(panelWidthRef.current);
    }
}, []);
```
Here panelRootRef is the ref of the PanelRoot component

## Panel Full Screen

The code of the panel full-screen logic is located at modules/dreamview_plus/frontend/packages/dreamview-core/src/components/panels/base/RenderTile/index.tsx
The code related to entering full screen is as follows:

```TypeScript
const enterFullScreen = useCallback(async () => {
    if (inFullScreen) return;

    const fullScreenHookConfig = hooksManager.getHook(panelId);
    let hookBeforeResBoolean = false;

    if (fullScreenHookConfig?.beforeEnterFullScreen) {
        hookBeforeResBoolean = await hooksManager.handleFullScreenBeforeHook(
            fullScreenHookConfig.beforeEnterFullScreen,
        );
    }

    const isExecutable = !fullScreenHookConfig?.beforeEnterFullScreen || hookBeforeResBoolean;

    if (isExecutable) {
        _setFullScreen(true);

        if (fullScreenHookConfig?.afterEnterFullScreen) {
            fullScreenHookConfig?.afterEnterFullScreen();
        }
    }
}, [inFullScreen, panelId]);
```

Entering and exiting full screen mainly update the full-screen state by calling _setFullScreen.
In addition, configuration of hook functions for entering and exiting full screen is also supported. The hooksManager is mainly used to manage the enter and exit full-screen hook functions of different panels.
In Panel, the related hook functions are registered by using registerFullScreenHooks:

```TypeScript
const registerFullScreenHooks = useCallback((hookConfig: FullScreenHookConfig) => {
    hooksManager.addHook(panelId, hookConfig);
}, []);
```

Pass in enterFullScreen and exitFullScreen through <PanelTileProvider />:

```TypeScript
<PanelTileProvider
    path={path}
    enterFullScreen={enterFullScreen}
    exitFullScreen={exitFullScreen}
    fullScreenFnObj={fullScreenFnRef.current}
>
    ...
</PanelTileProvider>
```

Obtain them in Panel through usePanelTileContext:
```TypeScript
  const { enterFullScreen, exitFullScreen, fullScreenFnObj } = usePanelTileContext();
```

## Shortcut Key Registration

The interfaces related to a panel's shortcut key registration are mainly setKeyDownHandlers and removeKeyDownHandlers.
setKeyDownHandlers saves the current Panel's shortcut key state and the state in the store:

```TypeScript
const setKeyDownHandlers = useCallback((handlers: KeyHandlers[]) => {
    _setKeyDownHandlers((prevHandlers) => [...prevHandlers, ...handlers]);
    // save

    // eslint-disable-next-line no-restricted-syntax
    for (const handler of handlers) {
        if (handler?.isGloable) {
            panelInfoDispatch(addGloableKeyHandler([handler]));
        } else {
            panelInfoDispatch(addKeyHandler({ panelId, keyHandlers: [handler] }));
        }
    }
}, []);
```

The shortcut key registration logic is mainly implemented in the <KeyListener /> component, and the registration mainly relies on rxjs's fromEvent:
```TypeScript
useEffect(() => {
    if (keyDownHandlers) {
        const len = keyDownHandlers.length;
        for (let i = 0; i < len; i += 1) {
            const keyDownEvent = fromEvent(document, 'keydown');
            const setKeyDown = getMultiPressedKey(keyDownEvent);
            const keyDownHandler = keyDownHandlers[i];
            setKeyDown(
                (event) => {
                    if (activeRef.current) {
                        keyDownHandler?.handler(event as KeyboardEvent);
                    }
                },
                keyDownHandler?.keys,
                keyDownHandler?.functionalKey,
            );
        }
    }

    return () => {
        // Want to clear all callbacks registered in keyDownEventRef.current
        for (const sp of subscriptionsRef.current) {
            sp.unsubscribe();
        }

        subscriptionsRef.current = [];
    };
}, [keyDownHandlers]);
```
The main logic of the above code is as follows:
1. Get the keydown event through fromEvent
2. Obtain a setKeyDown that can configure shortcut key event callbacks through getMultiPressedKey
3. Use setKeyDown to configure the related shortcut keys and the corresponding callback functions (the callback logic is executed only when the current panel is selected, that is, activeRef.current)
4. getMultiPressedKey acts like a factory function, used to produce the shortcut key configuration functions for each event

## Panel Selection

Panel selection/activation relies on the onClick event of <PanelRoot id={panelId} style={style} onClick={onPanleClick} ref={panelRootRef} />, and the handling logic is as follows:

```TypeScript
const onPanleClick: React.MouseEventHandler<HTMLDivElement> = useCallback(() => {
    if (!isSelected) {
        panelInfoDispatch(addSelectedPanelId(panelId));
    }
}, [isSelected, panelInfoDispatch]);
```

Updating the panel's activated state is implemented by updating selectedPanelId in PanelInfoStore.
UI logic location: modules/dreamview_plus/frontend/packages/dreamview-core/src/components/panels/base/RenderTile/index.tsx

```TypeScript
const isSelected = useMemo(() => panelInfoState?.selectedPanelIds?.has(panelId) ?? false, [panelInfoState]);

...

const finalCls = `${cx(classes['panel-item-container'], {
    [classes.dragging]: dragging,
})} ${classNames('panel-container', {
    'panel-selected': isSelected,
})}`;
```

## Channel Selector

The Channel selector provides Panel with the ability to switch Channels.
The main logic is located at: modules/dreamview_plus/frontend/packages/dreamview-core/src/components/panels/base/ChannelSelect/demo.tsx

```TypeScript
function DemoChannelSelect(props: RenderToolbarProps) {
    
    ...

    const channels = useMemo(() => {
        if (!curMeta) {
            return [];
        }

        return curMeta.channels.map((channel) => ({
            label: channel.channelName,
            value: channel.channelName,
        }));
    }, [curMeta]);
    const notifyInitialChannel = useRegisterNotifyInitialChanel(panelId);

    const onChange = (value, option) => {
        setCurVal(value);
        updateChannel({
            name: props.name,
            channel: value,
            needChannel: true,
        });
        localStorage.setItem(`${panelId}-selected-cn`, value);
    };

    useEffect(() => {
        if (channels.length > 0) {
            const cacheChannel = localStorage.getItem(`${panelId}-selected-cn`);
            let cn = channels[0]?.value;
            for (const channel of channels) {
                if (cacheChannel === channel.value) {
                    cn = cacheChannel;
                    break;
                }
            }
            setCurVal(cn);
            notifyInitialChannel({
                name: curMeta.dataName,
                channel: cn,
                needChannel: true,
            });
        } else {
            setCurVal(undefined);
        }
    }, [channels]);

    return <ChannelSelect value={curVal} options={channels} onChange={onChange} />;
}
```

The main logic is to cache on every switch and call updateChannel to publish an event to update the Channel, notifying Pannel to update the Channel.
On the first load, the cached Channel is taken first, and otherwise the first one in the channels array is taken as the default value.
