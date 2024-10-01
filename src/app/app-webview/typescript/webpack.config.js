const path = require('path');

module.exports = {
  mode: 'development',
  entry: {
    SettingsPage: './src/SettingsPage.tsx',
  },
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: 'ts-loader',
        exclude: /node_modules/,
      },
    ],
  },
  resolve: {
    extensions: ['.tsx', '.ts', '.js'],
  },
  devtool: "inline-source-map",
  watchOptions: {
    /* If no manual code change is needed, no need to rebuild;
     * otherwise, no need to race with the code generator for
     * file locks if the build is going to fail anyway */
    ignored: 'generated/**',
  },
};