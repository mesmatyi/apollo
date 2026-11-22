## @dreamview/dreamview-theme
##### 1. What is dreamview-theme
dreamview-theme is the complete set of Dreamview theme style variables. Each variable is given a semantic meaning, so that raw CSS styles can be avoided during development and styles are managed in a unified way.
##### 2. Usage
```
import {Provider as ThemeProvider, makeStyles} from '@dreamview/dreamview-theme';

const useStyle = makeStyles((theme) => {
    root: {
        backgroundColor: theme.tokens.backgroundColor.main,
        color: theme.tokens.font.color.main,
    },
});

function ChildComponents() {
    const {classes} = useStyle();
    
    return (<div className={classes.root}>root</div>)
}

function App(){
    return (
        <ThemeProvider>
            <ChildComponents />
        </ThemeProvider>    
    )
}
```
##### 3. Custom themes
A theme is replaced by overriding the values of the tokens variables.
```
import {Provider as ThemeProvider, makeStyles} from '@dreamview/dreamview-theme';

const useStyle = makeStyles((theme) => {
    root: {
        backgroundColor: theme.tokens.backgroundColor.main,
        color: theme.tokens.font.color.main,
    },
})
function ChildComponents() {
    const {classes} = useStyle();

    return (<div className={classes.root}>root</div>)
}

function App(){
    const customTheme = {
        tokens: {
            backgroundColor: {
                main: 'blue'            
            }        
        }    
    }
    return (
        <ThemeProvider config={customTheme}>
            <ChildComponents />
        </ThemeProvider>    
    )
}
```
##### 4. Using components. dremview-colors provides a collection of predefined styles, including component styles and some functional styles. It is still being improved, and we call it components
```
import {Provider as ThemeProvider, makeStyles} from '@dreamview/dreamview-theme';

// Here the button component style is used as an example
const useStyle = makeStyles((theme) => {
    root: theme.components.button.primary,
})

function ChildComponents() {
    const {classes} = useStyle();
    
    return (<button className={classes.root}>root</button>)
}

function App(){
    return (
        <ThemeProvider>
            <ChildComponents />
        </ThemeProvider>    
    )
}
```
##### 5. Development
```
├── README.md
├── createStyles.tsx // Creates the provider and context
├── main.ts // Entry file
├── scripts
│   └── generateType.js // Script that generates type definitions
├── tokens // Collection of variables
│   ├── baseToken // Base variables
│   │   ├── allColors.js // Complete set of theme colors
│   │   ├── colors.ts // Derives 10 similar colors from each theme color
│   │   ├── index.ts // Base variables
│   │   └── type.ts // 
│   └── components // components
│       ├── button.ts // Styles of the button component
│       └── index.ts
```
- Add a theme color
	1. Add the theme color in allColors.js
	2. Run the command to generate the type file: yarn lerna run generate:types --scope=@dreamview/dreamview-theme
- Modify/add/delete base variables
	1. Just modify the tokens/baseToke/index.ts file
- Add a collection of styles
	1. Create a folder under tokens/components and write your own style collection
	2. Export the folder you created in tokens/components/index.ts
