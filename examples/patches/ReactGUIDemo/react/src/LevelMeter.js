import React, { Component } from 'react';
import { Image, View, EventBridge } from 'juce-blueprint';

class LevelMeter extends Component {
  constructor(props) {
    super(props);

    this._onMeasure = this._onMeasure.bind(this);
    this._onOutgoingEvent = this._onOutgoingEvent.bind(this);

    this.state = {
      width: 0,
      height: 0,
      leftLevel : 0.0,
      rightLevel : 0.0
    };
  }

  componentDidMount() {
    EventBridge.addListener('outgoingEvent', this._onOutgoingEvent);
  }

  componentWillUnmount() {
    EventBridge.removeListener('outgoingEvent', this._onOutgoingEvent);
  }

  setMeterLevels(leftLevel, rightLevel) {
    this.setState({ leftLevel, rightLevel });
  }

  _onOutgoingEvent (frame, endpointID, value)
  {
    if (endpointID == "levelOut")
      this.setMeterLevels (value, value);
  }

  _onMeasure(e) {
    this.setState({
      width: e.width,
      height: e.height,
    });
  }

  _renderMeter(leftLevel, rightLevel, width, height) {
    return `
      <svg
        width="${width}" height="${height}" viewBox="0 0 ${width} ${height}"
        version="1.1" xmlns="http://www.w3.org/2000/svg">
        <rect
          x="${0}" y="${0}"
          width="${width}" height="${height}"
          fill="#333344" />
        <rect
          x="${width * 0.02}" y="${height * 0.05}"
          width="${width * 0.96 * leftLevel}" height="${height * 0.4}"
          fill="#44ddcc" />
        <rect
          x="${width * 0.02}" y="${height * 0.55}"
          width="${width * 0.96 * rightLevel}" height="${height * 0.4}"
          fill="#44ddcc" />
      </svg>
    `;
  }

  render() {
    const {leftLevel, rightLevel, width, height} = this.state;

    return (
      <View {...this.props} onMeasure={this._onMeasure}>
        <Image {...styles.canvas} source={this._renderMeter(leftLevel, rightLevel, width, height)} />
      </View>
    );
  }
}

const styles = {
  canvas: {
    'flex': 1.0,
    'height': '100%',
    'width': '100%',
    'position': 'absolute',
    'left': 0.0,
    'top': 0.0,
    'interceptClickEvents': false,
  },
};

export default LevelMeter;
