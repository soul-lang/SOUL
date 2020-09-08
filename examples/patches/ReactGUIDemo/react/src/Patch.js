import React, { Component } from 'react';
import ParameterControl from './ParameterControl';
import { View, Text, } from 'juce-blueprint';


class Patch extends Component {
  constructor(props) {
    super(props);

    this.paramIDs = global.getAllParameterIDs();

    console.log ("\nSOUL Patch Manifest: " + JSON.stringify (getManifest(), null, 2));
    console.log ("\nPatch Description: " + JSON.stringify (getPatchDescription(), null, 2));
    console.log ("\nInput Event Endpoint IDs: " + JSON.stringify (getInputEventEndpointIDs(), null, 2));
    console.log ("\nOutput Event Endpoint IDs: " + JSON.stringify (getOutputEventEndpointIDs(), null, 2));
    console.log ("\nParameter IDs: " + JSON.stringify (this.paramIDs, null, 2));
  }

  render() {
    return (
      <View {...styles.container}>
        <View {...styles.paramHolder}>
          { this.paramIDs.map (paramID => (
            <ParameterControl paramID={paramID} key={paramID} {...styles.paramControl}/>
          )) }
        </View>
      </View>
    );
  }
}

const styles = {
  container: {
    'width': '100%',
    'height': '100%',
    'background-color': 'ff17191f',
    'justify-content': 'center',
    'align-items': 'center',
  },
  paramHolder: {
    'flex': 1.0,
    'flex-direction': 'row',
    'flex-wrap': 'wrap',
    'justify-content': 'flex-start',
    'align-items': 'flex-start',
    'align-content': 'flex-start',
    'padding': '4%',
  },
};

export default Patch;
