import ParameterChangeNotifier from './ParameterChangeNotifier';
import React, { Component } from 'react';
import {
  Image,
  Text,
  View,
} from 'juce-blueprint';


class Slider extends Component {
  constructor(props) {
    super(props);

    this._onMeasure = this._onMeasure.bind(this);
    this._onMouseDown = this._onMouseDown.bind(this);
    this._onMouseUp = this._onMouseUp.bind(this);
    this._onMouseDrag = this._onMouseDrag.bind(this);
    this._renderVectorGraphics = this._renderVectorGraphics.bind(this);
    this._onParameterValueChange = this._onParameterValueChange.bind(this);

    // During a drag, we hold the value at which the drag started here to
    // ensure smooth behavior while the component state is being updated.
    this._valueAtDragStart = 0.0;

    this.paramState = global.getParameterState(this.props.paramID);

    const initialValue = typeof this.paramState.init === 'number' ? this.paramState.init : 0.0;

    this.state = {
      width: 0,
      height: 0,
      value: initialValue,
    };
  }

  componentDidMount() {
    ParameterChangeNotifier.addParameterListener (this.props.paramID, this._onParameterValueChange);
  }

  componentWillUnmount() {
    ParameterChangeNotifier.removeParameterListener (this.props.paramID, this._onParameterValueChange);
  }

  _onMeasure(e) {
    this.setState({
      width: e.width,
      height: e.height,
    });
  }

  _onMouseDown(e) {
    this._valueAtDragStart = this.state.value;
    this._mouseDownX = e.x;
    this._mouseDownY = e.y;
    global.beginParameterChangeGesture(this.props.paramID);
  }

  _onMouseUp(e) {
    global.endParameterChangeGesture(this.props.paramID);
  }

  _onMouseDrag(e) {
    // Component vectors
    let dx = e.x - this._mouseDownX;
    let dy = e.y - this._mouseDownY;

    // Delta
    let dm = dx - dy;
    const range = (this.paramState.max - this.paramState.min);
    const sensitivity = (range / 200.0);
    const value = Math.max (this.paramState.min, Math.min (this.paramState.max, this._valueAtDragStart + dm * sensitivity));

    if (typeof this.props.paramID === 'string' && this.props.paramID.length > 0) {
      global.setParameterValue(this.props.paramID, value);
    }
  }

  _onParameterValueChange(paramID) {
    const value = global.getParameterValue(paramID);

    this.setState({
      value: value.value,
      stringValue: value.stringValue
    });
  }

  _renderVectorGraphics() {
    const {width, height} = this.state;
    const cx = width * 0.5;
    const cy = height * 0.5;
    const strokeWidth = 3.0;

    // Note that we nudge the radius by half the stroke width; this is because
    // the stroke will extend outwards in both directions from the given coordinates,
    // which gets clipped if we try to draw the circle perfectly on the edge of the
    // image. We nudge it in so that no part of the path gets clipped.
    const radius = Math.max (1.0, (Math.min(width, height) * 0.5) - (strokeWidth / 2));

    // Animate the arc by stroke-dasharray, where the length of the dash is
    // related to the value property and the length of the space takes up the
    // rest of the circle.
    const arcCircumference = 1.5 * Math.PI * radius;
    const range = (this.paramState.max - this.paramState.min);
    const scaledValue = Math.max (0.0, Math.min (1.0, (this.state.value - this.paramState.min) / range));
    const dashArray = [scaledValue * arcCircumference, 2.0 * Math.PI * radius];

    return `
      <svg
        width="${width}"
        height="${height}"
        viewBox="0 0 ${width} ${height}"
        version="1.1"
        xmlns="http://www.w3.org/2000/svg">
        <circle
          cx="${cx}"
          cy="${cy}"
          r="${radius}"
          stroke="#626262"
          stroke-width="${strokeWidth}"
          stroke-dasharray="${[arcCircumference, arcCircumference].join(',')}"
          fill="none" />
        <circle
          cx="${cx}"
          cy="${cy}"
          r="${radius}"
          stroke="#66FDCF"
          stroke-width="${strokeWidth}"
          stroke-dasharray="${dashArray.join(',')}"
          fill="none" />
      </svg>
    `;
  }

  render() {
    return (
      <View
        {...this.props}
        onMeasure={this._onMeasure}
        onMouseDown={this._onMouseDown}
        onMouseUp={this._onMouseUp}
        onMouseDrag={this._onMouseDrag}>
        <Image {...styles.canvas} source={this._renderVectorGraphics()} />
        {this.props.children}
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
    'transform-rotate': Math.PI * 1.25,
  },
};

export default Slider;
