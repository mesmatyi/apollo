# Store Component Documentation

## Introduction

The Store component is the core part of our DreamviewPlus client and is responsible for managing and maintaining the application's state. It adopts modern front-end architecture concepts, combining reactive programming and immutable state management, to provide developers with an efficient and reliable state management solution.

## Component Structure

- **Reducer.ts** - Defines the Reducer functions that handle state changes and the related Action interfaces.
- **Store.ts** - Implements the Store, manages the application's state, and provides interfaces for updating state.
- **Provider.ts** - Provides a React component used to inject the Store into the React application.
- **Persistor.ts** - Responsible for state persistence, ensuring the continuity of the application state across sessions.
- **Middleware.ts** - Implements middleware, used to execute additional logic during state updates, such as asynchronous operation handling.
- **Factory.tsx** - Provides factory methods for creating and configuring Store instances, especially suitable for React environments.

## Feature Description

### Store.ts

- Implements reactive state management using rxjs's BehaviorSubject.
- Provides the ability to subscribe to state, allowing components to update automatically when the state changes.
- Integrates Persistor and Middleware, enhancing the functionality of state management.

### Reducer.ts

- Defines the `Action` and `PayloadAction` interfaces, providing standardized operation instructions for state updates.
- Each Reducer function decides how to update the state according to the received action type and payload.

### Provider.ts

- Uses React's Context API to inject the Store into React components.
- Enables React components to access the state and the dispatch method in the Store through Context.

### Persistor.ts

- Implements the state persistence logic.
- Ensures the consistency of the state across browser sessions through mechanisms such as LocalStorage.

### Middleware.ts

- Provides a mechanism to insert custom logic during state updates.
- Commonly used to handle asynchronous operations such as API requests.

### Factory.tsx

- Provides a highly configurable way to create Store instances.
- Makes integrating the Store in a React environment more flexible and convenient.

## Usage

### Creating and Configuring the Store

In `Factory.tsx`, we provide a method for creating Store instances. Developers can configure the Reducer, Middleware, and Persistor according to their needs. The following is an example of creating a Store instance:

```javascript
import { createStoreProvider } from './Factory.tsx';
const { StoreProvider, useStore } = createStoreProvider<IInitState, TYPES.CombineAction>({
    initialState: initState,
    reducer,
});

const AppWithStore = () => {
    return (
        <StoreProvider>
            <ChildComponent />
        </StoreProvider>
    );
};
```

## Use Cases

### State Updates

Suppose we have an action for synchronizing user information. First, define and handle this action in `action.ts`:

```typescript
import {PayloadAction} from '../base/Reducer';
import {INIT_USER_INFO} from './actionTypes';

export type IInitState = {
    userInfo: {
        avatar_url: string;
        displayname: string;
        id: string;
    };
    isLogin: boolean;
};

type InitUserInfoAction = PayloadAction<typeof INIT_USER_INFO, IInitState>;

export const initUserInfo = (payload: IInitState): InitUserInfoAction => ({
    type: INIT_USER_INFO,
    payload,
});

export type CombineAction = InitUserInfoAction;
```

Generate `StoreProvider` and `useStore` through the factory function:

```typescript
import { Factory } from '../base';
import { IInitState, CombineAction } from './actions';
import { initState, reducer } from './reducer';

export const { StoreProvider: UserInfoStoreProvider, useStore: useUserInfoStore } = Factory.createStoreProvider<
    IInitState,
    CombineAction
>({
    initialState: initState,
    reducer,
});
```

Then use `useStore` in the component to get the state and the dispatch method to update the state:

```typescript
 const [store, dispatch] = useUserInfoStore();

dispatch({
        userInfo: {
            avatar_url: undefined,
            displayname: undefined,
            id: undefined,
        },
        isLogin: true,
    },
);
```

### Asynchronous Operations

An asynchronous Middleware is already preset, so we can define asynchronous actions, such as synchronizing data after requesting an API:

```typescript
export const changeMode = (
    mainApi: MainApi,
    payload: TYPES.ChangeModePayload,
    callback?: (mode: CURRENT_MODE) => void,
): AsyncAction<TYPES.IInitState, ChangeModeAction> => {
    noop();
    return async (dispatch, state) => {
        logger.debug('changeMode', { state, payload });
        await mainApi.changeSetupMode(payload);
        if (callback) {
            callback(payload);
        }
    };
};
```

### Custom Middleware

The framework supports pluggable middleware definitions, and `[Middleware.ts](Middleware.ts)` already presets 4 types of middleware, namely:
- Asynchronous action handling middleware
- Logging middleware
- Crash handling middleware
- Synchronous debugging middleware

How do you define a middleware? Taking the asynchronous action handling middleware as an example, we first need to define a middleware function:

```typescript
export function asyncActionMiddleware<S, A>(
    store: Store<S, A>,
    next: (action: A) => void,
    action: AsyncAction<S, A> | A,
): void {
    if (typeof action === 'function') {
        (action as AsyncAction<S, A>)(store.dispatch, store.getState());
    } else {
        next(action);
    }
}
```

Then, in the factory function that initializes the Store, [Factory.tsx](Factory.tsx), pass in the middleware function:
```typescript
  store.applyMiddleware(...(props.middleware || middleware), ...middlewareExtra);
```

### State Persistence
To make it convenient to quickly restore state, we provide a state persistence feature. In [Factory.tsx](Factory.tsx), we can configure the persistence method by passing in `persistor`:

```typescript
 store.persist(iPersistor);
```

Persistence methods

In [Persistor.ts](Persistor.ts), we provide a factory function for creating persistence methods, and developers can configure the persistence method according to their needs:

```typescript
 createLocalStoragePersistor('dv-panel-inner-stoer');
```



## Conclusion

This document provides a detailed description of the Store component, including its feature description, code structure, usage, and concrete cases. We hope this document helps open-source developers better understand and use this component and contribute to the development and maintenance of the Dreamview Plus application. Feedback and suggestions of any kind are welcome.
