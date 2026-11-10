// https://docs.expo.dev/guides/using-eslint/
const { defineConfig } = require('eslint/config');
const expoConfig = require('eslint-config-expo/flat');

module.exports = defineConfig([
  expoConfig,
  {
    ignores: ['dist/*', 'src/data/catalog.generated.ts', 'src/theme/metrics.generated.ts'],
  },
  {
    // These two rules exist for the React Compiler, which this app does not use.
    // Gesture state is kept in refs that follow the latest render, and overlays
    // mount themselves from an effect before animating in.
    rules: {
      'react-hooks/refs': 'off',
      'react-hooks/set-state-in-effect': 'off',
    },
  },
]);
