import React, { Component } from 'react';
import Slider from './Slider';
import { View, Text, } from 'juce-blueprint';
import ParameterValueDisplay from './ParameterValueDisplay';

class ParameterControl extends Component {
  constructor(props) {
    super(props);

    this.paramState = getParameterState (props.paramID);
  }

  render() {
    const paramID = this.paramState.ID;

    return (
      <View {...styles.paramControl}>
        <Slider paramID={paramID} {...styles.knob}>
          <ParameterValueDisplay paramID={paramID} {...styles.valueDisplay} />
        </Slider>
        <Text {...styles.paramNameDisplay}>{this.paramState.name}</Text>
      </View>
    );
  }
}

const styles = {
    paramControl: {
        'flex': 0.0,
        'flex-direction': 'column',
        'justify-content': 'center',
        'align-items': 'center',
        'color': 'ff888888',
        'font-size': 16.0,
        'width': 120,
        'height': 120,
    },
    knob: {
        'width': '100%',
        'height': '80%',
    },
    valueDisplay: {
        'flex': 1.0,
        'justify-content': 'center',
        'align-items': 'center',
        'interceptClickEvents': false,
    },
    paramNameDisplay: {
        'flex': 1.0,
        'justify-content': 'center',
        'align-items': 'center',
        'interceptClickEvents': false,
        'color': 'ff666666',
        'font-size': 15.0,
        'height': 16.0
    }
};

export default ParameterControl;
