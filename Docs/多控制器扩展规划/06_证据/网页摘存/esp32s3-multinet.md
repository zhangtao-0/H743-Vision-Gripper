- [Home](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html)
- Command Word
- [Edit on GitHub](https://github.com/espressif/esp-sr/blob/78a923d/docs/en/speech_command_recognition/README.rst)

* * *

# Command Word [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#command-word "Permalink to this heading")

[\[中文\]](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/speech_command_recognition/README.html)

## MultiNet Command Word Recognition Model [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#multinet-command-word-recognition-model "Permalink to this heading")

MultiNet is a lightweight model designed to recognize multiple speech command words offline based on ESP32-S3. Currently, up to 200 speech commands, including customized commands, are supported.

- Support Chinese and English speech commands recognition

- Support user-defined commands

- Support adding / deleting / modifying commands during operation

- Up to 200 commands are supported

- It supports single recognition and continuous recognition

- Lightweight and low resource consumption

- Low delay, within 500ms

- Support online Chinese and English model switching (esp32s3 only)

- The model is partitioned separately to support users to apply OTA


The MultiNet input is the audio processed by the audio-front-end algorithm (AFE), with the format of 16 KHz, 16 bit and mono. By recognizing the audio signals, speech commands can be recognized.

Please refer to [Models Benchmark](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html) to check models supported by Espressif SoCs.

For details on flash models, see Section [Flashing Models](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/flash_model/README.html) .

Note

Models ending with Q8 represents the 8 bit version of the model, which is more lightweight.

## Commands Recognition Process [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#commands-recognition-process "Permalink to this heading")

Please see the flow diagram for commands recognition below:

![speech_command-recognition-system](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/_images/multinet_workflow.png)

speech\_command-recognition-system [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html#id1 "Permalink to this image")

## Speech Commands Customization Methods [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#speech-commands-customization-methods "Permalink to this heading")

Note

Mixed Chinese and English is not supported in command words.

The command word cannot contain Arabic numerals and special characters.

Please refer to Chinese version documentation for Chinese speech commands customization methods.

### MultiNet7 customize speech commands [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#multinet7-customize-speech-commands "Permalink to this heading")

MultiNet7 use phonemes for English speech commands. Please modify a text file [model/multinet\_model/fst/commands\_en.txt](https://github.com/espressif/esp-sr/blob/78a923d/model/multinet_model/fst/commands_en.txt) by the following format:

> ```
> # command_id,command_grapheme,command_phoneme
> 1,tell me a joke,TfL Mm c qbK
> 2,sing a song,Sgl c Sel
> ```
>
> Copy to clipboard

- Column 1: command ID, it should start from 1 and cannot be set to 0.

- Column 2: command\_grapheme, the command sentence. It is recommended to use lowercase letters unless it is an acronym that is meant to be pronounced differently.

- Column 3: command\_phoneme, the phoneme sequence of the command which is optional. To fill this column, please use [tool/multinet\_g2p.py](https://github.com/espressif/esp-sr/blob/78a923d/tool/multinet_g2p.py) to do the Grapheme-to-Phoneme conversion and paste the results at the third column correspondingly (this is the recommended way).


If Column 3 is left empty, then an internal Grapheme-to-Phoneme tool will be called at runtime. But there might be a little accuracy drop in this way due the different Grapheme-to-Phoneme algorithms used.

### MultiNet6 customize speech commands [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#multinet6-customize-speech-commands "Permalink to this heading")

MultiNet6 use grapheme for English speech commands, you can add/modify speech commands by words directly. Please modify a text file [model/multinet\_model/fst/commands\_en.txt](https://github.com/espressif/esp-sr/blob/78a923d/model/multinet_model/fst/commands_en.txt) by the following format:

> ```
> # command_id,command_grapheme
> 1,TELL ME A JOKE
> 2,MAKE A COFFEE
> ```
>
> Copy to clipboard

- Column 1: command ID, it should start from 1 and cannot be set to 0.

- Column 2: command\_grapheme, the command sentence. It is recommended to use all capital letters.


The extra column in the default commands\_en.txt is to keep it compatible with MultiNet7, there is no need to fill the third column when using MultiNet6.

### MultiNet5 customize speech commands [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#multinet5-customize-speech-commands "Permalink to this heading")

MultiNet5 use phonemes for English speech commands. For simplicity, we use characters to denote different phonemes. Please use [tool/multinet\_g2p.py](https://github.com/espressif/esp-sr/blob/78a923d/tool/multinet_g2p.py) to do the convention.

- Via `menuconfig`


> 1. Navigate to `idf.py menuconfig` \> `ESP Speech Recognition` \> `Add Chinese speech commands/Add English speech commands` to add speech commands. For details, please refer to the example in ESP-Skainet.
>
>
> ![menuconfig_add_speech_commands](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/_images/menuconfig_add_speech_commands.png)
>
> menuconfig\_add\_speech\_commands [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html#id2 "Permalink to this image")
>
> Please note that a single `Command ID` can correspond to more than one commands. For example, “da kai kong tiao” and “kai kong tiao” have the same meaning. Therefore, users can assign the same command id to these two commands and separate them with “,” (no space required before and after).
>
> 2. Call the following API:
>
>
> ```
> /**
>     * @brief Update the speech commands of MultiNet by menuconfig
>     *
>     * @param multinet            The multinet handle
>     *
>     * @param model_data          The model object to query
>     *
>     * @param language            The language of MultiNet
>     *
>     * @return
>     *     - ESP_OK                  Success
>     *     - ESP_ERR_INVALID_STATE   Fail
>     */
>     esp_err_t esp_mn_commands_update_from_sdkconfig(esp_mn_iface_t *multinet, const model_iface_data_t *model_data);
> ```
>
> Copy to clipboard


### Customize Speech Commands Via API calls [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#customize-speech-commands-via-api-calls "Permalink to this heading")

Alternatively, speech commands can be modified via API calls, this method works for MultiNet5, MultiNet6 and MultiNet7.

MutiNet5 requires the input command string to be phonemes, and MultiNet6 and MultiNet7 only accepts grapheme inputs to API calls.

- Apply new changes, the add/remove/modify/clear actions will not take effect util this function is called.


> ```
> /**
> * @brief Update the speech commands of MultiNet
> *
> * @Warning: Must be used after [add/remove/modify/clear] function,
> *           otherwise the language model of multinet can not be updated.
> *
> * @return
> *     - NULL                 Success
> *     - others               The list of error phrase which can not be parsed by multinet.
> */
> esp_mn_error_t *esp_mn_commands_update();
> ```
>
> Copy to clipboard
>
> Note
>
> The modifications will not be applied, thus not printed out, until you call `esp_mn_commands_update()`.

- Add a new speech command, will return `ESP_ERR_INVALID_STATE` if the input string is not in the correct format.


> ```
> /**
> * @brief Add one speech commands with command string and command ID
> *
> * @param command_id      The command ID
> * @param string  The command string of the speech commands
> *
> * @return
> *     - ESP_OK                  Success
> *     - ESP_ERR_INVALID_STATE   Fail
> */
> esp_err_t esp_mn_commands_add(int command_id, char *string);
> ```
>
> Copy to clipboard

- Remove a speech command, will return `ESP_ERR_INVALID_STATE` if the command does not exist.


> ```
> /**
> * @brief Remove one speech commands by command string
> *
> * @param string  The command string of the speech commands
> *
> * @return
> *     - ESP_OK                  Success
> *     - ESP_ERR_INVALID_STATE   Fail
> */
> esp_err_t esp_mn_commands_remove(char *string);
> ```
>
> Copy to clipboard

- Modify a speech command, will return `ESP_ERR_INVALID_STATE` if the command does not exist.


> ```
> /**
> * @brief Modify one speech commands with new command string
> *
> * @param old_string  The old command string of the speech commands
> * @param new_string  The new command string of the speech commands
> *
> * @return
> *     - ESP_OK                  Success
> *     - ESP_ERR_INVALID_STATE   Fail
> */
> esp_err_t esp_mn_commands_modify(char *old_string, char *new_string);
> ```
>
> Copy to clipboard

- Clear all speech commands.


> ```
> /**
> * @brief Clear all speech commands in linked list
> *
> * @return
> *     - ESP_OK                  Success
> *     - ESP_ERR_INVALID_STATE   Fail
> */
> esp_err_t esp_mn_commands_clear(void);
> ```
>
> Copy to clipboard

- Print cached speech commands, this function will print out all cached speech commands. Cached speech commands will be applied after `esp_mn_commands_update()` is called.


> ```
> /**
> * @brief Print all commands in linked list.
> */
> void esp_mn_commands_print(void);
> ```
>
> Copy to clipboard

- Print active speech commands, this function will print out all active speech commands.


> ```
> /**
> * @brief Print all commands in linked list.
> */
> void esp_mn_active_commands_print(void);
> ```
>
> Copy to clipboard


## Use MultiNet [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#use-multinet "Permalink to this heading")

We suggest to use MultiNet together with audio front-end (AFE) in ESP-SR. For details, see Section [AFE Introduction and Use](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html) .

After configuring AFE, users can follow the steps below to configure and run MultiNet.

### Initialize MultiNet [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#initialize-multinet "Permalink to this heading")

- Load and initialize MultiNet. For details, see Section [flash\_model](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/flash_model/README.html)

- Customize speech commands. For details, see Section [Speech Commands Customization Methods](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html#command-requirements)


### Run MultiNet [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#run-multinet "Permalink to this heading")

Users can start MultiNet after enabling AFE and WakeNet, but must pay attention to the following limitations:

- The frame length of MultiNet must be equal to the AFE fetch frame length

- The audio format supported is 16 KHz, 16 bit, mono. The data obtained by AFE fetch is also in this format


- Get the length of frame that needs to pass to MultiNet


> ```
> int mu_chunksize = multinet->get_samp_chunksize(model_data);
> ```
>
> Copy to clipboard
>
> `mu_chunksize` describes the `short` of each frame passed to MultiNet. This size is exactly the same as the number of data points per frame obtained in AFE.

- Start the speech recognition


> We send the data from AFE `fetch` to the following API:
>
> ```
> esp_mn_state_t mn_state = multinet->detect(model_data, buff);
> ```
>
> Copy to clipboard


The length of `buff` is `mu_chunksize * sizeof(int16_t)`.


### MultiNet Output [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#multinet-output "Permalink to this heading")

Speech command recognition must be used with WakeNet. After wake-up, MultiNet detection can start.

After running, MultiNet returns the recognition output of the current frame in real time `mn_state`, which is currently divided into the following identification states:

- ESP\_MN\_STATE\_DETECTING


> Indicates that the MultiNet is detecting but the target speech command word has not been recognized.

- ESP\_MN\_STATE\_DETECTED


> Indicates that the target speech command has been recognized. At this time, the user can call `get_results` interface to obtain the recognition results.
>
> ```
> esp_mn_results_t *mn_result = multinet->get_results(model_data);
> ```
>
> Copy to clipboard
>
> The recognition result is stored in the return value of the `get_result` API in the following format:
>
> ```
> typedef struct{
> esp_mn_state_t state;
>  int num;                // The number of phrase in list, num<=5. When num=0, no phrase is recognized.
>  int phrase_id[ESP_MN_RESULT_MAX_NUM];      // The list of phrase id.
>  float prob[ESP_MN_RESULT_MAX_NUM];         // The list of probability.
> } esp_mn_results_t;
> ```
>
> Copy to clipboard
>
> where,
>
>   - `state` is the recognition status of the current frame
>
>   - `num` means the number of recognized commands, `num` <= 5, up to 5 possible results are returned
>
>   - `phrase_id` means the Phrase ID of speech commands
>
>   - `prob` means the recognition probability of the recognized entries, which is arranged from large to small
>
>
> Users can use `phrase_id[0]` and `prob[0]` get the recognition result with the highest probability.

- ESP\_MN\_STATE\_TIMEOUT


> Indicates the speech commands has not been detected for a long time and will exit automatically and wait to be waked up again.


Single recognition mode and Continuous recognition mode:
\\* Single recognition mode: exit the speech recognition when the return status is `ESP_MN_STATE_DETECTED`
\\* Continuous recognition mode: exit the speech recognition when the return status is `ESP_MN_STATE_TIMEOUT`

## Resource Occupancy [](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html\#resource-occupancy "Permalink to this heading")

For the resource occupancy for this model, see [Resource Occupancy](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html).

[Provide feedback about this document](https://www.espressif.com/en/company/documents/documentation_feedback?docId=6473&sections=Command%20Word%20(speech_command_recognition/README)&version=latest%20(master))